#pragma once

#include "cueband.h"

#include <cstdint>
#include "drivers/SpiNorFlash.h"
#include <littlefs/lfs.h>

namespace Pinetime {
  namespace Controllers {
    class FS {
    public:
      FS(Pinetime::Drivers::SpiNorFlash&);

      void Init();
      void LVGLFileSystemInit();

      int FileOpen(lfs_file_t* file_p, const char* fileName, const int flags);
      int FileClose(lfs_file_t* file_p);
      int FileRead(lfs_file_t* file_p, uint8_t* buff, uint32_t size);
      int FileWrite(lfs_file_t* file_p, const uint8_t* buff, uint32_t size);
      int FileSeek(lfs_file_t* file_p, uint32_t pos);
#ifdef CUEBAND_FS_FILESIZE_ENABLED
      int FileSize(lfs_file_t* file_p);
#endif
#ifdef CUEBAND_FS_FILETELL_ENABLED
      int FileTell(lfs_file_t* file_p);
#endif

      int FileDelete(const char* fileName);

      int DirCreate(const char* path);
      int DirDelete(const char* path);

      void VerifyResource();

    private:

      Pinetime::Drivers::SpiNorFlash& flashDriver;

      /*
      * External Flash MAP (4 MBytes)
      *
      * 0x000000 +---------------------------------------+
      *          |  Bootloader Assets                    |
      *          |  256 KBytes                           |
      *          |                                       |
      * 0x040000 +---------------------------------------+
      *          |  OTA                                  |
      *          |  464 KBytes                           |
      *          |                                       |
      *          |                                       |
      *          |                                       |
      * 0x0B4000 +---------------------------------------+
      *          |  File System                          |
      *          |                                       |
      *          |                                       |
      *          |                                       |
      *          |                                       |
      * 0x400000 +---------------------------------------+
      *
      */
      static constexpr size_t startAddress = 0x0B4000;
#ifdef CUEBAND_POSSIBLE_FIX_FS
      // This value looks like it might've been incorrect (unless this variable isn't really the size in bytes?) 
      // ...anyway, this value is just smaller so shouldn't do any harm.
      static constexpr size_t size = 0x34C000;    // (0x400000 - 0x0B4000 =)
#else
      static constexpr size_t size = 0x3C0000;
#endif
      static constexpr size_t blockSize = 4096;

      bool resourcesValid = false;
      const struct lfs_config lfsConfig;

      lfs_t lfs;

      static int SectorSync(const struct lfs_config* c);
      static int SectorErase(const struct lfs_config* c, lfs_block_t block);
      static int SectorProg(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, const void* buffer, lfs_size_t size);
      static int SectorRead(const struct lfs_config* c, lfs_block_t block, lfs_off_t off, void* buffer, lfs_size_t size);

    };
  }
}
