#include "fat.h"
#include "filesystem/filesystem.h"
#include "result.h"
#include "types.h"

void PHX_Filesystem_FAT_BuildShortName(const PHX_Byte rawName[11], char out[13])
{
    PHX_Byte base[8];
    memcpy(base, rawName, 8);
    if (base[0] == PHX_FILESYSTEM_FAT_ENTRY_KANJI_ESCAPE)
        base[0] = 0xE5;

    const PHX_Byte* ext = rawName + 8;

    int len = 0;
    for (int i = 0; i < 8 && base[i] != ' '; i++)
        out[len++] = (char)base[i];

    if (ext[0] != ' ')
    {
        out[len++] = '.';
        for (int i = 0; i < 3 && ext[i] != ' '; i++)
            out[len++] = (char)ext[i];
    }

    out[len] = '\0';
}

void PHX_Filesystem_FAT_GetShortNameCharacters(const char* name, const char* nameEnd, PHX_Byte outCount, char* out)
{
    PHX_Byte charIndex = 0;
    while (*name && charIndex < outCount && (!nameEnd || name < nameEnd))
    {
        if (*name >= 'a' && *name <= 'z')
            out[charIndex++] = *name + (char)('A' - 'a');
        else if (*name >= 'A' && *name <= 'Z')
            out[charIndex++] = *name;
        else if (*name >= '0' && *name <= '9')
            out[charIndex++] = *name;
        else if (*name == '_' || *name == '$' || *name == '%' || *name == '\'' ||
                 *name == '@' || *name == '~' || *name == '!' || *name == '(' ||
                 *name == ')'  || *name == '{' || *name == '}' || *name == '^' ||
                 *name == '#'  || *name == '&')
            out[charIndex++] = *name;
        else if (charIndex > 0)
            out[charIndex++] = '_';
        name++;
    }
}

PHX_Bool PHX_Filesystem_FAT_NameEquals(const char* a, const char* b)
{
    // TODO: Actually use UTF8

    while (*a && *b)
    {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z')
            ca += (char)('a' - 'A');
        if (cb >= 'A' && cb <= 'Z')
            cb += (char)('a' - 'A');

        if (ca != cb)
            return PHX_FALSE;

        a++;
        b++;
    }

    return (*a == *b) ? PHX_TRUE : PHX_FALSE;
}

static PHX_Result shortNameExists(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char candidate[11], PHX_Bool* existsOut)
{
    PHX_Result result;
    PHX_Filesystem_FAT_Data* data = fs->data;
    PHX_Filesystem_FAT_Node_Extra* extra = dir->extra;

    const PHX_Bool rootDirectory = (dir->number == PHX_FILESYSTEM_FAT_NODE_NUMBER_ROOT && data->version != PHX_FILESYSTEM_FAT_32);

    if (rootDirectory)
    {
        for (PHX_u16 i = 0; i < data->specific.fat12_16.rootDirEntryCount; i++)
        {
            PHX_Byte entry[PHX_FILESYSTEM_FAT_DIRENT_SIZE];
            if ((result = PHX_Filesystem_FAT_ReadRootDirectoryEntries(data, i, 1, entry)) != PHX_SUCCESS)
                return result;

            if (*(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_FREE)
            {
                *existsOut = PHX_FALSE;
                return PHX_SUCCESS;
            }
            if (*(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_DELETED || *(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) == PHX_FILESYSTEM_FAT_LFN_ATTRIBUTE)
                continue;

            if (memcmp(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM, candidate, 11) == 0)
            {
                *existsOut = PHX_TRUE;
                return PHX_SUCCESS;
            }
        }

        *existsOut = PHX_FALSE;
        return PHX_SUCCESS;
    }

    PHX_u32 cluster = extra->startCluster;
    PHX_u32 status;

    while ((status = PHX_Filesystem_FAT_Cluster(data->version, cluster)) == PHX_FILESYSTEM_FAT_CLUSTER_NORMAL)
    {
        if (data->usedDevice->read(data->usedDevice, data->clusterBuffer, PHX_Filesystem_FAT_GetClusterStart(data, cluster), data->sectorsPerCluster) != data->sectorsPerCluster)
            return PHX_ERROR_IO;

        const PHX_u32 entriesPerCluster = data->bytesPerCluster / PHX_FILESYSTEM_FAT_DIRENT_SIZE;
        
        for (PHX_u32 i = 0; i < entriesPerCluster; i++)
        {
            PHX_Byte* entry = data->clusterBuffer + i * PHX_FILESYSTEM_FAT_DIRENT_SIZE;

            if (*(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_FREE)
            {
                *existsOut = PHX_FALSE;
                return PHX_SUCCESS;
            }
            if (*(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM) == PHX_FILESYSTEM_FAT_ENTRY_DELETED || *(entry + PHX_FILESYSTEM_FAT_DIRENT_ATR) == PHX_FILESYSTEM_FAT_LFN_ATTRIBUTE)
                continue;

            if (memcmp(entry + PHX_FILESYSTEM_FAT_DIRENT_NAM, candidate, 11) == 0)
            {
                *existsOut = PHX_TRUE;
                return PHX_SUCCESS;
            }
        }

        if ((result = PHX_Filesystem_FAT_ReadFAT(data, cluster, &cluster)) != PHX_SUCCESS)
            return result;
    }

    if (status != PHX_FILESYSTEM_FAT_CLUSTER_EOC)
        return PHX_ERROR_FORMAT;

    *existsOut = PHX_FALSE;
    return PHX_SUCCESS;
}

PHX_Result PHX_Filesystem_FAT_GenerateShortName(PHX_Filesystem* fs, PHX_Filesystem_Node* dir, const char firstChars[8], const char hash[4], const char ext[3], char shortNameOut[11])
{
    static const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_$%'@~!(){}^#&";
    static const size_t alphabetSize = sizeof(alphabet) - 1;

    /*
        1.  8 chars
        2.  6 chars + ~n
        3.  6 chars + n~
        4.  2 chars + hash + ~n
        5.  2 chars + hash + n~
        6.  1 char + hash + ~nn
        7.  1 char + hash + n~n
        8.  1 char + hash + nn~
        9.  hash + ~nnn
        10. hash + n~nn
        11. hash + nn~n
        12. hash + nnn~
    */

    PHX_Result result;
    PHX_Bool exists;
    PHX_u32 counter;
    char shortName[11];

    // 1
    memcpy(shortName, firstChars, 8);
    memcpy(shortName + 8, ext, 3);
    if (shortName[0] == ' ')
        shortName[0] = '_';
    if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
        return result;

    if (exists != PHX_TRUE)
        goto found;


    for (int i = 0; i < 6; i++)
    {
        if (shortName[i] == ' ') shortName[i] = '#';
    }
    
    
    // 2
    counter = 0;
    shortName[6] = '~';
    while (counter < alphabetSize)
    {
        shortName[7] = alphabet[counter++];

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }

    // 3
    counter = 0;
    shortName[7] = '~';
    while (counter < alphabetSize)
    {
        shortName[6] = alphabet[counter++];

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;
        if (exists != PHX_TRUE)
            goto found;
    }


    // 4
    counter = 0;
    memcpy(shortName + 2, hash, 4);
    shortName[6] = '~';
    while (counter < alphabetSize)
    {
        shortName[7] = alphabet[counter++];

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }

    // 5
    counter = 0;
    shortName[7] = '~';
    while (counter < alphabetSize)
    {
        shortName[6] = alphabet[counter++];

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;
        if (exists != PHX_TRUE)
            goto found;
    }


    // 6
    counter = 0;
    memcpy(shortName + 1, hash, 4);
    shortName[5] = '~';
    while (counter < alphabetSize * alphabetSize)
    {
        shortName[6] = alphabet[counter / alphabetSize];
        shortName[7] = alphabet[counter % alphabetSize];
        counter++;

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }

    // 7
    counter = 0;
    shortName[6] = '~';
    while (counter < alphabetSize * alphabetSize)
    {
        shortName[5] = alphabet[counter / alphabetSize];
        shortName[7] = alphabet[counter % alphabetSize];
        counter++;

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }

    // 8
    counter = 0;
    shortName[7] = '~';
    while (counter < alphabetSize * alphabetSize)
    {
        shortName[5] = alphabet[counter / alphabetSize];
        shortName[6] = alphabet[counter % alphabetSize];
        counter++;

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }


    // 9
    counter = 0;
    memcpy(shortName, hash, 4);
    shortName[4] = '~';
    while (counter < alphabetSize * alphabetSize * alphabetSize)
    {
        shortName[5] = alphabet[counter / (alphabetSize * alphabetSize)];
        shortName[6] = alphabet[(counter % (alphabetSize * alphabetSize)) / alphabetSize];
        shortName[7] = alphabet[counter % alphabetSize];
        counter++;

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }

    // 10
    counter = 0;
    shortName[5] = '~';
    while (counter < alphabetSize * alphabetSize * alphabetSize)
    {
        shortName[4] = alphabet[counter / (alphabetSize * alphabetSize)];
        shortName[6] = alphabet[(counter % (alphabetSize * alphabetSize)) / alphabetSize];
        shortName[7] = alphabet[counter % alphabetSize];
        counter++;

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }

    // 11
    counter = 0;
    shortName[6] = '~';
    while (counter < alphabetSize * alphabetSize * alphabetSize)
    {
        shortName[4] = alphabet[counter / (alphabetSize * alphabetSize)];
        shortName[5] = alphabet[(counter % (alphabetSize * alphabetSize)) / alphabetSize];
        shortName[7] = alphabet[counter % alphabetSize];
        counter++;

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }

    // 12
    counter = 0;
    shortName[7] = '~';
    while (counter < alphabetSize * alphabetSize * alphabetSize)
    {
        shortName[4] = alphabet[counter / (alphabetSize * alphabetSize)];
        shortName[5] = alphabet[(counter % (alphabetSize * alphabetSize)) / alphabetSize];
        shortName[6] = alphabet[counter % alphabetSize];
        counter++;

        if ((result = shortNameExists(fs, dir, shortName, &exists)) != PHX_SUCCESS)
            return result;

        if (exists != PHX_TRUE)
            goto found;
    }


    return PHX_ERROR_DIRECTORY;

found:
    memcpy(shortNameOut, shortName, 11);
    return PHX_SUCCESS;
}
