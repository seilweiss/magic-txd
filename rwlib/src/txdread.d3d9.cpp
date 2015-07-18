#include <StdInc.h>

#include "txdread.d3d9.hxx"

#include "pixelformat.hxx"

#include "txdread.common.hxx"

#include "txdread.d3d.dxt.hxx"

#include "pluginutil.hxx"

namespace rw
{

inline uint32 getCompressionFromD3DFormat( D3DFORMAT d3dFormat )
{
    uint32 compressionIndex = 0;

    if ( d3dFormat == D3DFMT_DXT1 )
    {
        compressionIndex = 1;
    }
    else if ( d3dFormat == D3DFMT_DXT2 )
    {
        compressionIndex = 2;
    }
    else if ( d3dFormat == D3DFMT_DXT3 )
    {
        compressionIndex = 3;
    }
    else if ( d3dFormat == D3DFMT_DXT4 )
    {
        compressionIndex = 4;
    }
    else if ( d3dFormat == D3DFMT_DXT5 )
    {
        compressionIndex = 5;
    }

    return compressionIndex;
}

void d3d9NativeTextureTypeProvider::DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const
{
    Interface *engineInterface = theTexture->engineInterface;

    {
        BlockProvider texNativeImageStruct( &inputProvider );

        texNativeImageStruct.EnterContext();

        try
        {
            if ( texNativeImageStruct.getBlockID() == CHUNK_STRUCT )
            {
                d3d9::textureMetaHeaderStructGeneric metaHeader;
                texNativeImageStruct.read( &metaHeader, sizeof(metaHeader) );

	            uint32 platform = metaHeader.platformDescriptor;

	            if (platform != PLATFORM_D3D9)
                {
                    throw RwException( "invalid platform type in Direct3D 9 texture reading" );
                }

                // Recast the texture to our native type.
                NativeTextureD3D9 *platformTex = (NativeTextureD3D9*)nativeTex;

                int engineWarningLevel = engineInterface->GetWarningLevel();

                bool engineIgnoreSecureWarnings = engineInterface->GetIgnoreSecureWarnings();

                // Read the texture names.
                {
                    char tmpbuf[ sizeof( metaHeader.name ) + 1 ];

                    // Make sure the name buffer is zero terminted.
                    tmpbuf[ sizeof( metaHeader.name ) ] = '\0';

                    // Move over the texture name.
                    memcpy( tmpbuf, metaHeader.name, sizeof( metaHeader.name ) );

                    theTexture->SetName( tmpbuf );

                    // Move over the texture mask name.
                    memcpy( tmpbuf, metaHeader.maskName, sizeof( metaHeader.maskName ) );

                    theTexture->SetMaskName( tmpbuf );
                }

                // Read texture format.
                metaHeader.texFormat.parse( *theTexture );

                // Deconstruct the format flags.
                bool hasMipmaps = false;    // TODO: actually use this flag.

                readRasterFormatFlags( metaHeader.rasterFormat, platformTex->rasterFormat, platformTex->paletteType, hasMipmaps, platformTex->autoMipmaps );

                platformTex->hasAlpha = false;

                // Read the D3DFORMAT field.
                D3DFORMAT d3dFormat = metaHeader.d3dFormat; // can be really anything.

                platformTex->d3dFormat = d3dFormat;

                uint32 depth = metaHeader.depth;
                uint32 maybeMipmapCount = metaHeader.mipmapCount;

                platformTex->depth = depth;

                platformTex->rasterType = metaHeader.rasterType;

                bool hasExpandedFormatRegion = ( metaHeader.isNotRwCompatible == true );    // LEGACY: "compression flag"

                {
                    // Here we decide about alpha.
	                platformTex->hasAlpha = metaHeader.hasAlpha;
                    platformTex->isCubeTexture = metaHeader.isCubeTexture;
                    platformTex->autoMipmaps = metaHeader.autoMipMaps;

	                if ( hasExpandedFormatRegion )
                    {
                        // If we are a texture with expanded format region, we can map to much more than original RW textures.
                        // We can be a compressed texture, or something entirely different that we do not know about.

                        // A really bad thing is that we cannot check the D3DFORMAT field for validity.
                        // There are countless undefined formats out there that we must be able to just "pass on".

		                // Detect FOUR-CC versions for compression method.
                        uint32 dxtCompression = getCompressionFromD3DFormat(d3dFormat);

                        platformTex->dxtCompression = dxtCompression;
                    }
	                else
                    {
                        // There is never compression in original RW.
		                platformTex->dxtCompression = 0;
                    }
                }

                // Verify raster properties and attempt to fix broken textures.
                // Broken textures travel with mods like San Andreas Retextured.
                // - Verify compression.
                {
                    uint32 actualCompression = getCompressionFromD3DFormat( d3dFormat );

                    if (actualCompression != platformTex->dxtCompression)
                    {
                        engineInterface->PushWarning( "texture " + theTexture->GetName() + " has invalid compression parameters (ignoring)" );

                        platformTex->dxtCompression = actualCompression;
                    }
                }
                // - Verify raster format.
                bool d3dRasterFormatLink = false;
                d3dpublic::nativeTextureFormatHandler *usedFormatHandler = NULL;
                {
                    eColorOrdering colorOrder = COLOR_BGRA;

                    bool isValidFormat = false;
                    bool isRasterFormatRequired = true;

                    eRasterFormat d3dRasterFormat;

                    bool isD3DFORMATImportant = true;

                    bool hasActualD3DFormat = false;
                    D3DFORMAT actualD3DFormat;

                    bool hasReportedStrongWarning = false;

                    // Do special logic for palettized textures.
                    // (thank you DK22Pac)
                    if (platformTex->paletteType != PALETTE_NONE)
                    {
                        // This overrides the D3DFORMAT field.
                        // We are forced to use the eRasterFormat property.
                        isD3DFORMATImportant = false;

                        colorOrder = COLOR_RGBA;

                        d3dRasterFormat = platformTex->rasterFormat;

                        hasActualD3DFormat = true;
                        actualD3DFormat = D3DFMT_P8;

                        isValidFormat = ( d3dFormat == D3DFMT_P8 );

                        // Basically, we have to tell the user that it should have had a palette D3DFORMAT.
                        if ( engineIgnoreSecureWarnings == false )
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " is a palette texture but did not set D3DFMT_P8" );

                            hasReportedStrongWarning = true;
                        }
                    }
                    else
                    {
                        // Set it for clarity sake.
                        // We do not load entirely complaint to GTA:SA, because we give higher priority to the D3DFORMAT field.
                        // Even though we do that, it is preferable, since the driver implementation is more powerful than the RW original types.
                        // TODO: add an interface property to enable GTA:SA-compliant loading behavior.
                        isD3DFORMATImportant = true;

                        if (d3dFormat == D3DFMT_A8R8G8B8)
                        {
                            d3dRasterFormat = RASTER_8888;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_X8R8G8B8)
                        {
                            d3dRasterFormat = RASTER_888;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_R8G8B8)
                        {
                            d3dRasterFormat = RASTER_888;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_R5G6B5)
                        {
                            d3dRasterFormat = RASTER_565;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_X1R5G5B5)
                        {
                            d3dRasterFormat = RASTER_555;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_A1R5G5B5)
                        {
                            d3dRasterFormat = RASTER_1555;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_A4R4G4B4)
                        {
                            d3dRasterFormat = RASTER_4444;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_A8B8G8R8)
                        {
                            d3dRasterFormat = RASTER_8888;

                            colorOrder = COLOR_RGBA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_X8B8G8R8)
                        {
                            d3dRasterFormat = RASTER_888;

                            colorOrder = COLOR_RGBA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_L8)
                        {
                            d3dRasterFormat = RASTER_LUM8;

                            // Actually, there is no such thing as a color order for luminance textures.
                            // We set this field so we make things happy.
                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;
                        }
                        else if (d3dFormat == D3DFMT_DXT1)
                        {
                            if (platformTex->hasAlpha)
                            {
                                d3dRasterFormat = RASTER_1555;
                            }
                            else
                            {
                                d3dRasterFormat = RASTER_565;
                            }

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;

                            isRasterFormatRequired = false;
                        }
                        else if (d3dFormat == D3DFMT_DXT2 || d3dFormat == D3DFMT_DXT3)
                        {
                            d3dRasterFormat = RASTER_4444;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;

                            isRasterFormatRequired = false;
                        }
                        else if (d3dFormat == D3DFMT_DXT4 || d3dFormat == D3DFMT_DXT5)
                        {
                            d3dRasterFormat = RASTER_4444;

                            colorOrder = COLOR_BGRA;

                            isValidFormat = true;

                            isRasterFormatRequired = false;
                        }
                        else if (d3dFormat == D3DFMT_P8)
                        {
                            // We cannot be a palette texture without having actual palette data.
                            isValidFormat = false;
                        }

                        // Is the D3DFORMAT known by this implementation?
                        // This is equivalent to the notion that we are a valid format.
                        if ( isValidFormat == false )
                        {
                            // We actually only do this if we have the extended format range enabled.
                            // TODO: make sure DK gets his converter weirdness sorted out.
                            //if ( hasExpandedFormatRegion )
                            {
                                // We could have a native format handler that has been registered to us as plugin.
                                // Try to look one up.
                                // If we have one, then we are known anyway!
                                d3dpublic::nativeTextureFormatHandler *formatHandler = this->GetFormatHandler( d3dFormat );

                                if ( formatHandler )
                                {
                                    // Lets just use this.
                                    usedFormatHandler = formatHandler;

                                    // Just use default raster format.
                                    d3dRasterFormat = RASTER_DEFAULT;

                                    colorOrder = COLOR_BGRA;

                                    // No required raster format.
                                    isRasterFormatRequired = false;

                                    isValidFormat = true;

                                    if ( hasExpandedFormatRegion == false )
                                    {
                                        // We kinda have a broken texture here.
                                        // This may not load on the GTA:SA engine.
                                        engineInterface->PushWarning( "texture " + theTexture->GetName() + " has extended D3DFORMAT link but does not enable 'isNotRwCompatible'" );
                                    }
                                }
                            }
                        }

                        // If everything else fails...
                        if ( isValidFormat == false )
                        {
                            // If the user wants to know about such things, notify him.
                            if ( engineIgnoreSecureWarnings == false )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an unknown D3DFORMAT link (" + std::to_string( (DWORD)d3dFormat ) + ")" );

                                hasReportedStrongWarning = true;
                            }

                            // There is an even graver error if the extended format range has been left disabled.
                            if ( hasExpandedFormatRegion == false )
                            {
                                engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an unknown D3DFORMAT link but does not enable 'isNotRwCompatible'" );
                            }
                        }
                    }

                    if ( isValidFormat == false )
                    {
                        // Fix it (if possible).
                        if ( hasActualD3DFormat )
                        {
                            d3dFormat = actualD3DFormat;

                            platformTex->d3dFormat = actualD3DFormat;

                            // We rescued ourselves into valid territory.
                            isValidFormat = true;
                        }
                    }

                    // If we are a valid format, we are actually known, which means we have a D3DFORMAT -> eRasterFormat link.
                    // This allows us to be handled by the Direct3D 9 RW implementation.
                    if ( isValidFormat )
                    {
                        // If the raster format is not required though, then it means that it actually has no link.
                        if ( isRasterFormatRequired )
                        {
                            d3dRasterFormatLink = true;
                        }
                    }
                    else
                    {
                        // If we are a valid format that we know, we also have a d3dRasterFormat that we want to enforce.
                        // Otherwise we are not entirely sure, so we should keep it as RASTER_DEFAULT.
                        d3dRasterFormat = RASTER_DEFAULT;
                    }
                    
                    eRasterFormat rasterFormat = platformTex->rasterFormat;

                    if ( rasterFormat != d3dRasterFormat )
                    {
                        // We should only warn about a mismatching format if we kinda know what we are doing.
                        // Otherwise we have already warned the user about the invalid D3DFORMAT entry, that we base upon anyway.
                        if ( hasReportedStrongWarning == false )
                        {
                            if ( isRasterFormatRequired || !engineIgnoreSecureWarnings )
                            {
                                if ( engineWarningLevel >= 3 )
                                {
                                    engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an invalid raster format (ignoring)" );
                                }
                            }
                        }

                        // Fix it.
                        platformTex->rasterFormat = d3dRasterFormat;
                    }

                    // Store the color ordering.
                    platformTex->colorOrdering = colorOrder;

                    // Store whether we have the D3D raster format link.
                    platformTex->d3dRasterFormatLink = d3dRasterFormatLink;

                    // Maybe we have a format handler.
                    platformTex->anonymousFormatLink = usedFormatHandler;
                }
                // - Verify depth.
                {
                    bool hasInvalidDepth = false;

                    if (platformTex->paletteType == PALETTE_4BIT)
                    {
                        if (depth != 4 && depth != 8)
                        {
                            hasInvalidDepth = true;
                        }
                    }
                    else if (platformTex->paletteType == PALETTE_8BIT)
                    {
                        if (depth != 8)
                        {
                            hasInvalidDepth = true;
                        }
                    }

                    if (hasInvalidDepth == true)
                    {
                        throw RwException( "texture " + theTexture->GetName() + " has an invalid depth" );

                        // We cannot fix an invalid depth.
                    }
                }

                if (platformTex->paletteType != PALETTE_NONE)
                {
                    // We kind assume we have a valid D3D raster format link here.
                    if ( d3dRasterFormatLink == false )
                    {
                        // If we do things correctly, this should never be triggered.
                        throw RwException( "texture " + theTexture->GetName() + " is a palette texture but has no Direct3D raster format link" );
                    }

                    uint32 reqPalItemCount = getD3DPaletteCount( platformTex->paletteType );

                    uint32 palDepth = Bitmap::getRasterFormatDepth( platformTex->rasterFormat );

                    assert( palDepth != 0 );

                    size_t paletteDataSize = getPaletteDataSize( reqPalItemCount, palDepth );

                    // Check whether we have palette data in the stream.
                    texNativeImageStruct.check_read_ahead( paletteDataSize );

                    void *palData = engineInterface->PixelAllocate( paletteDataSize );

                    try
                    {
	                    texNativeImageStruct.read( palData, paletteDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( palData );

                        throw;
                    }

                    // Store the palette.
                    platformTex->palette = palData;
                    platformTex->paletteSize = reqPalItemCount;
                }

                mipGenLevelGenerator mipLevelGen( metaHeader.width, metaHeader.height );

                if ( !mipLevelGen.isValidLevel() )
                {
                    throw RwException( "texture " + theTexture->GetName() + " has invalid dimensions" );
                }

                uint32 mipmapCount = 0;

                uint32 processedMipmapCount = 0;

                uint32 dxtCompression = platformTex->dxtCompression;

                bool hasDamagedMipmaps = false;

                for (uint32 i = 0; i < maybeMipmapCount; i++)
                {
                    bool couldEstablishLevel = true;

	                if (i > 0)
                    {
                        couldEstablishLevel = mipLevelGen.incrementLevel();
                    }

                    if (!couldEstablishLevel)
                    {
                        break;
                    }

                    // Create a new mipmap layer.
                    NativeTextureD3D9::mipmapLayer newLayer;

                    newLayer.layerWidth = mipLevelGen.getLevelWidth();
                    newLayer.layerHeight = mipLevelGen.getLevelHeight();

                    // Process dimensions.
                    uint32 texWidth = newLayer.layerWidth;
                    uint32 texHeight = newLayer.layerHeight;
                    {
		                // DXT compression works on 4x4 blocks,
		                // no smaller values allowed
		                if (dxtCompression != 0)
                        {
			                texWidth = ALIGN_SIZE( texWidth, 4u );
                            texHeight = ALIGN_SIZE( texHeight, 4u );
		                }
                    }

                    newLayer.width = texWidth;
                    newLayer.height = texHeight;

	                uint32 texDataSize = texNativeImageStruct.readUInt32();

                    // We started processing this mipmap.
                    processedMipmapCount++;

                    // Verify the data size.
                    // We can only do that if we know about its format.
                    bool isValidMipmap = true;

                    if ( d3dRasterFormatLink == true )
                    {
                        uint32 actualDataSize = 0;

                        if (dxtCompression != 0)
                        {
                            uint32 texItemCount = ( texWidth * texHeight );

                            actualDataSize = getDXTRasterDataSize(dxtCompression, texItemCount);
                        }
                        else
                        {
                            uint32 rowSize = getD3DRasterDataRowSize( texWidth, depth );

                            actualDataSize = getRasterDataSizeByRowSize(rowSize, texHeight);
                        }

                        if (actualDataSize != texDataSize)
                        {
                            isValidMipmap = false;
                        }
                    }
                    else
                    {
                        // Check some general stuff.
                        if ( texDataSize == 0 )
                        {
                            isValidMipmap = false;
                        }

                        if ( isValidMipmap )
                        {
                            // If we have a format plugin, make sure we match its size.
                            if ( usedFormatHandler != NULL )
                            {
                                uint32 shouldBeSize = usedFormatHandler->GetFormatTextureDataSize( texWidth, texHeight );

                                if ( texDataSize != shouldBeSize )
                                {
                                    isValidMipmap = false;
                                }
                            }
                        }
                    }

                    if ( !isValidMipmap )
                    {
                        // Even the Rockstar games texture generator appeared to have problems with mipmap generation.
                        // This is why textures appear to have the size of zero.

                        if (texDataSize != 0)
                        {
                            if ( !engineIgnoreSecureWarnings )
                            {
                               engineInterface->PushWarning( "texture " + theTexture->GetName() + " has damaged mipmaps (ignoring)" );
                            }

                            hasDamagedMipmaps = true;
                        }

                        // Skip the damaged bytes.
                        if (texDataSize != 0)
                        {
                            texNativeImageStruct.skip( texDataSize );
                        }
                        break;
                    }

                    // We first have to check whether there is enough data in the stream.
                    // Otherwise we would just flood the memory in case of an error;
                    // that could be abused by exploiters.
                    texNativeImageStruct.check_read_ahead( texDataSize );
                    
                    void *texelData = engineInterface->PixelAllocate( texDataSize );

                    try
                    {
	                    texNativeImageStruct.read( texelData, texDataSize );
                    }
                    catch( ... )
                    {
                        engineInterface->PixelFree( texelData );

                        throw;
                    }

                    // Store mipmap properties.
	                newLayer.dataSize = texDataSize;

                    // Store the image data pointer.
	                newLayer.texels = texelData;

                    // Put the layer.
                    platformTex->mipmaps.push_back( newLayer );

                    mipmapCount++;
                }
                
                if ( mipmapCount == 0 )
                {
                    throw RwException( "texture " + theTexture->GetName() + " is empty" );
                }

                // mipmapCount can only be smaller than maybeMipmapCount.
                // This is logically true and would be absurd to assert here.

                if ( processedMipmapCount < maybeMipmapCount )
                {
                    // Skip the remaining mipmaps (most likely zero-sized).
                    bool hasSkippedNonZeroSized = false;

                    for ( uint32 n = processedMipmapCount; n < maybeMipmapCount; n++ )
                    {
                        uint32 mipSize = texNativeImageStruct.readUInt32();

                        if ( mipSize != 0 )
                        {
                            hasSkippedNonZeroSized = true;

                            // Skip the section.
                            texNativeImageStruct.skip( mipSize );
                        }
                    }

                    if ( !engineIgnoreSecureWarnings && !hasDamagedMipmaps )
                    {
                        // Print the debug message.
                        if ( !hasSkippedNonZeroSized )
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " has zero sized mipmaps" );
                        }
                        else
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " violates mipmap rules" );
                        }
                    }
                }

                // Fix filtering mode.
                fixFilteringMode( *theTexture, mipmapCount );

                // - Verify auto mipmap
                {
                    bool hasAutoMipmaps = platformTex->autoMipmaps;

                    if ( hasAutoMipmaps )
                    {
                        bool canHaveAutoMipmaps = ( mipmapCount == 1 );

                        if ( !canHaveAutoMipmaps )
                        {
                            engineInterface->PushWarning( "texture " + theTexture->GetName() + " has an invalid auto-mipmap flag (fixing)" );

                            platformTex->autoMipmaps = false;
                        }
                    }
                }
            }
            else
            {
                engineInterface->PushWarning( "failed to find texture native image struct in D3D texture native" );
            }
        }
        catch( ... )
        {
            texNativeImageStruct.LeaveContext();

            throw;
        }

        texNativeImageStruct.LeaveContext();
    }

    // Read extensions.
    engineInterface->DeserializeExtensions( theTexture, inputProvider );
}

static PluginDependantStructRegister <d3d9NativeTextureTypeProvider, RwInterfaceFactory_t> d3dNativeTexturePluginRegister;

void registerD3D9NativePlugin( void )
{
    d3dNativeTexturePluginRegister.RegisterPlugin( engineFactory );
}

};