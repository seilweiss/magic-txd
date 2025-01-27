/*****************************************************************************
*
*  PROJECT:     Multi Theft Auto v1.2
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        FileSystem/src/fsinternal/CFileSystem.translator.pathutil.h
*  PURPOSE:     FileSystem path utilities for all kinds of translators
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*
*****************************************************************************/

#ifndef _FILESYSTEM_TRANSLATOR_PATHUTIL_
#define _FILESYSTEM_TRANSLATOR_PATHUTIL_

class CSystemPathTranslator : public virtual CFileTranslator
{
public:
                    CSystemPathTranslator           ( bool isSystemPath );
                    ~CSystemPathTranslator          ( void );

private:
    template <typename charType>
    bool            GenGetFullPathTreeFromRoot      ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            GenGetFullPathTree              ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            GenGetRelativePathTreeFromRoot  ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            GenGetRelativePathTree          ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            GenGetFullPathFromRoot          ( const charType *path, bool allowFile, filePath& output ) const;
    template <typename charType>
    bool            GenGetFullPath                  ( const charType *path, bool allowFile, filePath& output ) const;
    template <typename charType>
    bool            GenGetRelativePathFromRoot      ( const charType *path, bool allowFile, filePath& output ) const;
    template <typename charType>
    bool            GenGetRelativePath              ( const charType *path, bool allowFile, filePath& output ) const;
    template <typename charType>
    bool            GenChangeDirectory              ( const charType *path );

    // GetRelativePathTreeFromRoot
    // GetRelativePathFromRoot
    // GetFullPathTree
    // GetRelativePathTree

    template <typename charType>
    bool            IntGetRelativePathTreeFromRoot  ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            IntGetRelativePathFromRoot      ( const charType *path, bool allowFile, filePath& output ) const;
    template <typename charType>
    bool            IntGetFullPathTree              ( const charType *path, dirTree& tree, bool& file ) const;
    template <typename charType>
    bool            IntGetRelativePathTree          ( const charType *path, dirTree& tree, bool& file ) const;

public:
    bool            GetFullPathTreeFromRoot         ( const char *path, dirTree& tree, bool& file ) const override final;
    bool            GetFullPathTree                 ( const char *path, dirTree& tree, bool& file ) const override final;
    bool            GetRelativePathTreeFromRoot     ( const char *path, dirTree& tree, bool& file ) const override final;
    bool            GetRelativePathTree             ( const char *path, dirTree& tree, bool& file ) const override final;
    bool            GetFullPathFromRoot             ( const char *path, bool allowFile, filePath& output ) const override final;
    bool            GetFullPath                     ( const char *path, bool allowFile, filePath& output ) const override final;
    bool            GetRelativePathFromRoot         ( const char *path, bool allowFile, filePath& output ) const override final;
    bool            GetRelativePath                 ( const char *path, bool allowFile, filePath& output ) const override final;
    bool            ChangeDirectory                 ( const char *path ) override final;
    void            GetDirectory                    ( filePath& output ) const override final;

    bool            GetFullPathTreeFromRoot         ( const wchar_t *path, dirTree& tree, bool& file ) const override final;
    bool            GetFullPathTree                 ( const wchar_t *path, dirTree& tree, bool& file ) const override final;
    bool            GetRelativePathTreeFromRoot     ( const wchar_t *path, dirTree& tree, bool& file ) const override final;
    bool            GetRelativePathTree             ( const wchar_t *path, dirTree& tree, bool& file ) const override final;
    bool            GetFullPathFromRoot             ( const wchar_t *path, bool allowFile, filePath& output ) const override final;
    bool            GetFullPath                     ( const wchar_t *path, bool allowFile, filePath& output ) const override final;
    bool            GetRelativePathFromRoot         ( const wchar_t *path, bool allowFile, filePath& output ) const override final;
    bool            GetRelativePath                 ( const wchar_t *path, bool allowFile, filePath& output ) const override final;
    bool            ChangeDirectory                 ( const wchar_t *path ) override final;

    inline bool IsTranslatorRootDescriptor( wchar_t character ) const
    {
        if ( !m_isSystemPath )
        {
            // On Windows platforms, absolute paths are based on drive letters (C:/).
            // This means we can use the linux way of absolute fs root linkling for the translators.
            // But this may confuse Linux users; on Linux this convention cannot work (reserved for abs paths).
            // Hence, usage of the '/' or '\\' descriptor is discouraged (only for backwards compatibility).
            // Disregard this thought for archive file systems, all file systems which are the root themselves.
            // They use the linux addressing convention.
            if ( character == '/' || character == '\\' )
                return true;
        }

        // Just like MTA:SA is using the '' character for private directories, we use it
        // for a platform independent translator root basing.
        // 'textfile.txt' will address 'textfile.txt' in the translator root, ignoring
        // the current directory setting.
        return character == '' || character == '@';
    }

protected:
    friend class CFileSystem;
    friend struct CFileSystemNative;

    filePath        m_root;
    filePath        m_currentDir;
    dirTree         m_rootTree;
    dirTree         m_curDirTree;

    // Called to confirm current directory change.
    virtual bool    OnConfirmDirectoryChange( const dirTree& tree )
    {
        // Override this method to implement your own logic for changing directories.
        // You do not have to call back to this method.
        return true;
    }

    // Path resolution extension functions.
    // Override these to extend the way that this translator resolves paths.
    virtual bool    OnGetRelativePathTreeFromRoot( const char *path, dirTree& output, bool& file, bool& success ) const
    {
        return false;
    }
    virtual bool    OnGetRelativePathTreeFromRoot( const wchar_t *path, dirTree& output, bool& file, bool& success ) const
    {
        return false;
    }

    virtual bool    OnGetRelativePathTree( const char *path, dirTree& output, bool& file, bool& success ) const
    {
        return false;
    }
    virtual bool    OnGetRelativePathTree( const wchar_t *path, dirTree& output, bool& file, bool& success ) const
    {
        return false;
    }

    virtual bool    OnGetFullPathTree( const char *path, dirTree& output, bool& file, bool& success ) const
    {
        return false;
    }
    virtual bool    OnGetFullPathTree( const wchar_t *path, dirTree& output, bool& file, bool& success ) const
    {
        return false;
    }

    virtual void    OnGetFullPath( filePath& curAbsPath ) const
    {
        return;
    }

private:
    bool            m_isSystemPath;

    NativeExecutive::CReadWriteLock *lockPathConsistency;
};

#endif //_FILESYSTEM_TRANSLATOR_PATHUTIL_