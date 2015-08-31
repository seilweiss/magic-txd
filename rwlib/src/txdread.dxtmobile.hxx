#include "txdread.nativetex.hxx"

#include "txdread.d3d.genmip.hxx"

#define PLATFORMDESC_DXT_MOBILE 9

namespace rw
{

enum eS3TCInternalFormat
{
    COMPRESSED_RGB_S3TC_DXT1 = 0x83F0,
    COMPRESSED_RGBA_S3TC_DXT1 = 0x83F1,
    COMPRESSED_RGBA_S3TC_DXT3 = 0x83F2,
    COMPRESSED_RGBA_S3TC_DXT5 = 0x83F3
};

inline uint32 getDXTTypeFromS3TCInternalFormat( eS3TCInternalFormat internalFormat )
{
    uint32 dxtType = 0;

    if ( internalFormat == COMPRESSED_RGB_S3TC_DXT1 ||
         internalFormat == COMPRESSED_RGBA_S3TC_DXT1 )
    {
        dxtType = 1;
    }
    else if ( internalFormat == COMPRESSED_RGBA_S3TC_DXT3 )
    {
        dxtType = 3;
    }
    else if ( internalFormat == COMPRESSED_RGBA_S3TC_DXT5 )
    {
        dxtType = 5;
    }
    else
    {
        assert( 0 );
    }

    return dxtType;
}

struct NativeTextureMobileDXT
{
    Interface *engineInterface;

    LibraryVersion texVersion;

    inline NativeTextureMobileDXT( Interface *engineInterface )
    {
        this->engineInterface = engineInterface;
        this->texVersion = engineInterface->GetVersion();
        this->internalFormat = COMPRESSED_RGB_S3TC_DXT1;
        this->unk3 = 0;
        this->hasAlpha = false;
    }

    inline NativeTextureMobileDXT( const NativeTextureMobileDXT& right )
    {
        Interface *engineInterface = right.engineInterface;

        this->engineInterface = engineInterface;
        this->texVersion = right.texVersion;
        this->internalFormat = right.internalFormat;
        this->unk3 = right.unk3;
        this->hasAlpha = right.hasAlpha;

        // Copy mipmap layers.
        copyMipmapLayers( engineInterface, right.mipmaps, this->mipmaps );
    }

    inline void clearTexelData( void )
    {
        deleteMipmapLayers( this->engineInterface, this->mipmaps );
    }

    inline ~NativeTextureMobileDXT( void )
    {
        // Delete mipmap information.
        this->clearTexelData();
    }

    typedef genmip::mipmapLayer mipmapLayer;

    std::vector <mipmapLayer> mipmaps;

    eS3TCInternalFormat internalFormat;

    uint32 unk3;

    bool hasAlpha;
};

struct dxtMobileNativeTextureTypeProvider : public texNativeTypeProvider
{
    void ConstructTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        new (objMem) NativeTextureMobileDXT( engineInterface );
    }

    void CopyConstructTexture( Interface *engineInterface, void *objMem, const void *srcObjMem, size_t memSize ) override
    {
        new (objMem) NativeTextureMobileDXT( *(const NativeTextureMobileDXT*)srcObjMem );
    }
    
    void DestroyTexture( Interface *engineInterface, void *objMem, size_t memSize ) override
    {
        ( *(NativeTextureMobileDXT*)objMem ).~NativeTextureMobileDXT();
    }

    eTexNativeCompatibility IsCompatibleTextureBlock( BlockProvider& inputProvider ) const;

    void SerializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& outputProvider ) const;
    void DeserializeTexture( TextureBase *theTexture, PlatformTexture *nativeTex, BlockProvider& inputProvider ) const;

    void GetPixelCapabilities( pixelCapabilities& capsOut ) const override
    {
        capsOut.supportsDXT1 = true;
        capsOut.supportsDXT2 = false;
        capsOut.supportsDXT3 = true;
        capsOut.supportsDXT4 = false;
        capsOut.supportsDXT5 = true;
        capsOut.supportsPalette = true;
    }

    void GetStorageCapabilities( storageCapabilities& storeCaps ) const override
    {
        storeCaps.pixelCaps.supportsDXT1 = true;
        storeCaps.pixelCaps.supportsDXT2 = false;
        storeCaps.pixelCaps.supportsDXT3 = true;
        storeCaps.pixelCaps.supportsDXT4 = false;
        storeCaps.pixelCaps.supportsDXT5 = true;
        storeCaps.pixelCaps.supportsPalette = false;

        storeCaps.isCompressedFormat = true;
    }

    void GetPixelDataFromTexture( Interface *engineInterface, void *objMem, pixelDataTraversal& pixelsOut );
    void SetPixelDataToTexture( Interface *engineInterface, void *objMem, const pixelDataTraversal& pixelsIn, acquireFeedback_t& feedbackOut );
    void UnsetPixelDataFromTexture( Interface *engineInterface, void *objMem, bool deallocate );

    void SetTextureVersion( Interface *engineInterface, void *objMem, LibraryVersion version ) override
    {
        NativeTextureMobileDXT *nativeTex = (NativeTextureMobileDXT*)objMem;

        nativeTex->texVersion = version;
    }

    LibraryVersion GetTextureVersion( const void *objMem ) override
    {
        const NativeTextureMobileDXT *nativeTex = (const NativeTextureMobileDXT*)objMem;

        return nativeTex->texVersion;
    }

    bool GetMipmapLayer( Interface *engineInterface, void *objMem, uint32 mipIndex, rawMipmapLayer& layerOut );
    bool AddMipmapLayer( Interface *engineInterface, void *objMem, const rawMipmapLayer& layerIn, acquireFeedback_t& feedbackOut );
    void ClearMipmaps( Interface *engineInterface, void *objMem );

    void GetTextureInfo( Interface *engineInterface, void *objMem, nativeTextureBatchedInfo& infoOut );
    void GetTextureFormatString( Interface *engineInterface, void *objMem, char *buf, size_t bufLen, size_t& lengthOut ) const;

    eRasterFormat GetTextureRasterFormat( const void *objMem ) override
    {
        return RASTER_DEFAULT;
    }

    ePaletteType GetTexturePaletteType( const void *objMem ) override
    {
        return PALETTE_NONE;
    }

    bool IsTextureCompressed( const void *objMem ) override
    {
        return true;
    }

    bool DoesTextureHaveAlpha( const void *objMem ) override
    {
        const NativeTextureMobileDXT *nativeTex = (const NativeTextureMobileDXT*)objMem;

        return nativeTex->hasAlpha;
    }

    uint32 GetTextureDataRowAlignment( void ) const override
    {
        // Row alignment of raw data does not really matter.
        // We will compress to DXT anyway.
        return 0;
    }

    uint32 GetDriverIdentifier( void *objMem ) const override
    {
        // This was never defined.
        return 0;
    }

    inline void Initialize( Interface *engineInterface )
    {
        RegisterNativeTextureType( engineInterface, "s3tc_mobile", this, sizeof( NativeTextureMobileDXT ) );
    }

    inline void Shutdown( Interface *engineInterface )
    {
        UnregisterNativeTextureType( engineInterface, "s3tc_mobile" );
    }
};

namespace mobile_dxt
{
#pragma pack(push, 1)
struct textureNativeGenericHeader
{
    endian::little_endian <uint32> platformDescriptor;

    wardrumFormatInfo formatInfo;

    uint8 pad1[0x10];

    char name[32];
    char maskName[32];

    uint8 mipmapCount;
    uint8 unk1;
    bool hasAlpha;

    uint8 pad2;

    endian::little_endian <uint16> width, height;

    endian::little_endian <eS3TCInternalFormat> internalFormat;

    endian::little_endian <uint32> imageDataSectionSize;
    endian::little_endian <uint32> unk3;
};
#pragma pack(pop)
};

};