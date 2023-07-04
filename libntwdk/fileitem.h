#pragma once

typedef struct FILEITEM
{
    PWSTR pszFilename;
} FILEITEM;

struct __declspec(novtable) CFileItem : public FILEITEM
{
    CFileItem()
    {
        pszFilename = NULL;
    }

    CFileItem(WCHAR *Filename)
    {
		setName(Filename);
    }

    ~CFileItem()
    {
		freeItem();
    }

    HRESULT SetFile(WCHAR *Filename)
    {
		freeItem();
		setName(Filename);
        return 0;
    }

private:
	void setName(WCHAR *Filename)
	{
        pszFilename = _wcsdup(Filename);
	}

	void freeItem()
	{
		if( pszFilename != NULL ) {
			free(pszFilename);
			pszFilename = NULL;
		}
	}
};

class __declspec(novtable) CFileList
{
    FILEITEM **Files;
    int cFiles;
public:
    CFileList()
    {
        Files = NULL;
        cFiles = 0;
    }

    ~CFileList()
    {
		if( Files )
			FreeMemory(Files);
		cFiles = 0;
    }

    int GetCount() const
    {
        return cFiles;
    }

    int Add(CFileItem *pfi)
    {
        if( Files == NULL )
        {
            Files = (FILEITEM **)AllocMemory( sizeof(FILEITEM*) );
            if( Files == NULL )
                return -1;
        }
        else
        {
            FILEITEM** temp;
            temp = (FILEITEM**)ReallocMemory(Files,(sizeof(FILEITEM*) * (cFiles + 1)));
            if( temp == NULL )
            {
                free(Files);
                return -1;
            }
            Files = temp;
        }

        Files[ cFiles++ ] = pfi;

        return cFiles-1;
    }

    CFileItem* operator[](int i) const
    {
        if( i < cFiles )
            return (CFileItem*)Files[i];
        return NULL;
    }

    CFileItem **FirstFile() const
    {
        return (CFileItem **)&Files[0];
    }

    CFileItem **NextFile(CFileItem **ppfi) const
    {
        CFileItem **end = (CFileItem **)&Files[cFiles];
        if( (++ppfi) < end )
            return ppfi;
        return NULL;
    }
};
