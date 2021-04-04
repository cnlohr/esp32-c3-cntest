//Copyright 2015 <>< Charles Lohr Under the MIT/x11 License, NewBSD License or
// ColorChord License.  You Choose.

#include "mfs.h"
#include <esp_partition.h>

#ifdef USE_RAM_MFS
//#include "mfs_data.h"
#endif

#include <string.h>

static const char *TAG = "MFS";

#ifdef USE_RAM_MFS

uint8_t * mfs_data;
spi_flash_mmap_handle_t mfs_handle;

void MFSInit()
{
	if( !mfs_data )
	{
		spi_flash_mmap( 0x110000, 512*1024, SPI_FLASH_MMAP_DATA, (const void **) (&mfs_data), &mfs_handle);
	}
}

//Returns 0 on succses.
//Returns size of file if non-empty
//If positive, populates mfi.
//Returns -1 if can't find file or reached end of file list.
int8_t MFSOpenFile( const char * fname, struct MFSFileInfo * mfi )
{
	struct MFSFileEntry e;

	uint8_t * ptr = mfs_data;

	while(1)
	{
		//spi_flash_read( ptr, (uint32*)&e, sizeof( e ) );		
		memcpy( &e, ptr, sizeof( e ) );
		ptr += sizeof(e);
		if( e.name[0] == 0xff || strlen( e.name ) == 0 ) break;

		if( strcmp( e.name, fname ) == 0 )
		{
			mfi->offset = e.start;
			mfi->filelen = e.len;
			return 0;
		}
	}
	return -1;
}

int32_t MFSReadSector( uint8_t* data, struct MFSFileInfo * mfi )
{
	 //returns # of bytes left tin file.
	if( !mfi->filelen )
	{
		return 0;
	}

	int toread = mfi->filelen;
	if( toread > MFS_SECTOR ) toread = MFS_SECTOR;

	memcpy( data, &mfs_data[mfi->offset], toread );
	mfi->offset += toread;
	mfi->filelen -= toread;
	return mfi->filelen;
}

void MFSClose( struct MFSFileInfo * mfi )
{
}

#else

#include <esp_partition.h>
#include <wear_levelling.h>
#include <esp_vfs_fat.h>
#include <diskio_wl.h>

wl_handle_t fatfs_partition_handle;
FATFS * fatfs_handle;

void MFSInit()
{

	const esp_vfs_fat_mount_config_t mcfg = { 
		.format_if_mount_failed = 1,
		.max_files = 7,
		.allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
	};
	esp_err_t err = esp_vfs_fat_spiflash_mount( "/fatfs", "fatfs", &mcfg, &fatfs_partition_handle );
	if( err )
	{
		ESP_LOGE( TAG, "esp_vfs_fat_spiflash_mount(...) = %d\n", err );
		return;
	}
	ESP_LOGI( TAG, "FatFS Mounted\n" );
}


//Returns 0 on succses.
//Returns size of file if non-empty
//If positive, populates mfi.
//Returns -1 if can't find file or reached end of file list.
int8_t MFSOpenFile( const char * fname, struct MFSFileInfo * mfi )
{
	char targfile[1024];

	if( mfi->file )
	{
		ESP_LOGE( TAG, "File dobule-opened on mfi %p\n", mfi );
	}
	if( strlen( fname ) == 0 || fname[strlen(fname)-1] == '/' )
	{
		snprintf( targfile, sizeof( targfile ) - 1, "/fatfs/%s/index.html", fname );
	}
	else
	{
		snprintf( targfile, sizeof( targfile ) - 1, "/fatfs/%s", fname );
	}

	//printf( ":%s:%p\n", targfile, mfi );
	FILE * f = mfi->file = fopen( targfile, "rb" );
	if( !f ) return -1;
	//printf( "F: %p\n", f );
	fseek( f, 0, SEEK_END );
	mfi->filelen = ftell( f );
	fseek( f, 0, SEEK_SET );
	return 0;
}

int32_t MFSReadSector( uint8_t* data, struct MFSFileInfo * mfi )
{
	if( !mfi->filelen )
	{
		return 0;
	}

	int toread = fread( data, 1, MFS_SECTOR, mfi->file );
	mfi->filelen -= toread;
	return mfi->filelen;
}

void MFSClose( struct MFSFileInfo * mfi )
{
	//printf( "===== CLOSE MFI %p\n", mfi );
	if( mfi->file )
	{
		fclose( mfi->file );
		mfi->file = 0;
	}
}


#endif


