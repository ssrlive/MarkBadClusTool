/*
 * layout.h - Ntfs on-disk layout structures.  Originated from the Linux-NTFS project.
 *
 * Copyright (c) 2000-2005 Anton Altaparmakov
 * Copyright (c)      2005 Yura Pakhuchiy
 * Copyright (c) 2005-2006 Szabolcs Szakacsits
 */



//layout_ntfs.h
//author: modified by lzc
//date:2012/11/07
//e-mail:hackerlzc@126.com

#pragma once
#ifndef _LAYOUT_NTFS_H
#define _LAYOUT_NTFS_H

#include<windows.h>
#include<guiddef.h>

#pragma pack(push)
#pragma pack(1)
#pragma warning(push)
#pragma warning(disable:4200)


#define magicNTFS       (0x202020205346544eull)     /* "NTFS    " */
#define NTFS_SB_MAGIC    (0x5346544e)               /* 'NTFS' */

/**
 * struct BIOS_PARAMETER_BLOCK - BIOS parameter block (bpb) structure.
 */
typedef struct {
/*0x00*/WORD bytes_per_sector;              /* Size of a sector in bytes. */
/*0x02*/BYTE sectors_per_cluster;           /* Size of a cluster in sectors. */
/*0x03*/WORD reserved_sectors;              /* zero */
/*0x05*/BYTE fats;                          /* zero */
/*0x06*/WORD root_entries;                  /* zero */
/*0x08*/WORD sectors;                       /* zero */
/*0x0a*/BYTE media_type;                    /* 0xf8 = hard disk */
/*0x0b*/WORD sectors_per_fat;               /* zero */
/*0x0d*/WORD sectors_per_track;             /* Required to boot Windows. */
/*0x0f*/WORD heads;                         /* Required to boot Windows. */
/*0x11*/DWORD hidden_sectors;               /* Offset to the start of the partition
                                            relative to the disk in sectors.
                                            Required to boot Windows. */
/*0x15*/DWORD large_sectors;                /* zero */
/* sizeof() = 25 (0x19) bytes */
}BIOS_PARAMETER_BLOCK,*PBIOS_PARAMETER_BLOCK;

/**/
/**
 * struct NTFS_BOOT_SECTOR - NTFS boot sector structure.
 */
typedef struct {
/*0x00*/BYTE  jump[3];                      /* Irrelevant (jump to boot up code).*/
/*0x03*/DWORDLONG oem_id;                   /* Magic "NTFS    ". */
/*0x0b*/BIOS_PARAMETER_BLOCK bpb;           /* See BIOS_PARAMETER_BLOCK. */
/*0x24*/BYTE physical_drive;                /* 0x00 floppy, 0x80 hard disk */
/*0x25*/BYTE current_head;                  /* zero */
/*0x26*/BYTE extended_boot_signature;       /* 0x80 */
/*0x27*/BYTE reserved2;                     /* zero */
/*0x28*/DWORDLONG number_of_sectors;        /* Number of sectors in volume. Gives
                                            maximum volume size of 2^63 sectors.
                                            Assuming standard sector size of 512
                                            bytes, the maximum byte size is
                                            approx. 4.7x10^21 bytes. (-; */
/*0x30*/DWORDLONG mft_lcn;                  /* Cluster location of mft data. */
/*0x38*/DWORDLONG mftmirr_lcn;              /* Cluster location of copy of mft. */
/*0x40*/CHAR  clusters_per_mft_record;      /* Mft record size in clusters. */
/*0x41*/BYTE  reserved0[3];                 /* zero */
/*0x44*/CHAR  clusters_per_index_record;    /* Index block size in clusters. */
/*0x45*/BYTE  reserved1[3];                 /* zero */
/*0x48*/DWORDLONG volume_serial_number;     /* Irrelevant (serial number). */
/*0x50*/DWORD checksum;                     /* Boot sector checksum. */
/*0x54*/BYTE  bootstrap[426];               /* Irrelevant (boot up code). */
/*0x1fe*/WORD end_of_sector_marker;         /* End of bootsector magic. Always is
                                            0xaa55 in little endian. */
/* sizeof() = 512 (0x200) bytes */
}NTFS_BOOT_SECTOR,*PNTFS_BOOT_SECTOR;


/**
 * enum NTFS_RECORD_TYPES -
 *
 * Magic identifiers present at the beginning of all ntfs record containing
 * records (like mft records for example).
 */
typedef enum:ULONG {
    /* Found in $MFT/$DATA. */
    magic_FILE = 0x454c4946ul,              /* Mft entry. */
    magic_INDX = 0x58444e49ul,              /* Index buffer. */
    magic_HOLE = 0x454c4f48ul,              /* ? (NTFS 3.0+?) */

    /* Found in $LogFile/$DATA. */
    magic_RSTR = 0x52545352ul,              /* Restart page. */
    magic_RCRD = 0x44524352ul,              /* Log record page. */

    /* Found in $LogFile/$DATA.  (May be found in $MFT/$DATA, also?) */
    magic_CHKD = 0x444b4843ul,              /* Modified by chkdsk. */

    /* Found in all ntfs record containing records. */
    magic_BAAD = 0x44414142ul,              /* Failed multi sector transfer was detected. */

    /*
     * Found in $LogFile/$DATA when a page is full or 0xff bytes and is
     * thus not initialized.  User has to initialize the page before using
     * it.
     */
    magic_empty = 0xfffffffful,             /* Record is empty and has
                                            to be initialized before
                                            it can be used. */
} NTFS_RECORD_TYPES;

/*
 * Generic magic comparison macros. Finally found a use for the ## preprocessor
 * operator! (-8
 */
#define ntfs_is_magic(x, m)    (   (DWORD)(x) == (DWORD)magic_##m )
#define ntfs_is_magicp(p, m)    ( *(DWORD*)(p) == (DWORD)magic_##m )

/*
 * Specialised magic comparison macros for the NTFS_RECORD_TYPES defined above.
 */
#define ntfs_is_file_record(x)    ( ntfs_is_magic (x, FILE) )
#define ntfs_is_file_recordp(p)    ( ntfs_is_magicp(p, FILE) )
#define ntfs_is_mft_record(x)    ( ntfs_is_file_record(x) )
#define ntfs_is_mft_recordp(p)    ( ntfs_is_file_recordp(p) )
#define ntfs_is_indx_record(x)    ( ntfs_is_magic (x, INDX) )
#define ntfs_is_indx_recordp(p)    ( ntfs_is_magicp(p, INDX) )
#define ntfs_is_hole_record(x)    ( ntfs_is_magic (x, HOLE) )
#define ntfs_is_hole_recordp(p)    ( ntfs_is_magicp(p, HOLE) )

#define ntfs_is_rstr_record(x)    ( ntfs_is_magic (x, RSTR) )
#define ntfs_is_rstr_recordp(p)    ( ntfs_is_magicp(p, RSTR) )
#define ntfs_is_rcrd_record(x)    ( ntfs_is_magic (x, RCRD) )
#define ntfs_is_rcrd_recordp(p)    ( ntfs_is_magicp(p, RCRD) )

#define ntfs_is_chkd_record(x)    ( ntfs_is_magic (x, CHKD) )
#define ntfs_is_chkd_recordp(p)    ( ntfs_is_magicp(p, CHKD) )

#define ntfs_is_baad_record(x)    ( ntfs_is_magic (x, BAAD) )
#define ntfs_is_baad_recordp(p)    ( ntfs_is_magicp(p, BAAD) )

#define ntfs_is_empty_record(x)        ( ntfs_is_magic (x, empty) )
#define ntfs_is_empty_recordp(p)    ( ntfs_is_magicp(p, empty) )


#define NTFS_BLOCK_SIZE        512
#define NTFS_BLOCK_SIZE_BITS    9

/**
 * struct NTFS_RECORD -
 *
 * The Update Sequence Array (usa) is an array of the WORD values which belong
 * to the end of each sector protected by the update sequence record in which
 * this array is contained. Note that the first entry is the Update Sequence
 * Number (usn), a cyclic counter of how many times the protected record has
 * been written to disk. The values 0 and -1 (ie. 0xffff) are not used. All
 * last WORD's of each sector have to be equal to the usn (during reading) or
 * are set to it (during writing). If they are not, an incomplete multi sector
 * transfer has occurred when the data was written.
 * The maximum size for the update sequence array is fixed to:
 *    maximum size = usa_ofs + (usa_count * 2) = 510 bytes
 * The 510 bytes comes from the fact that the last WORD in the array has to
 * (obviously) finish before the last WORD of the first 512-byte sector.
 * This formula can be used as a consistency check in that usa_ofs +
 * (usa_count * 2) has to be less than or equal to 510.
 */
typedef struct {
    NTFS_RECORD_TYPES magic;                /* A four-byte magic identifying the
                                            record type and/or status. */
    WORD usa_ofs;                           /* Offset to the Update Sequence Array (usa)
                                            from the start of the ntfs record. */
    WORD usa_count;                         /* Number of WORD sized entries in the usa
                                            including the Update Sequence Number (usn),
                                            thus the number of fixups is the usa_count
                                            minus 1. */
}NTFS_RECORD,*PNTFS_RECORD;

/**
 * enum NTFS_SYSTEM_FILES - System files mft record numbers.
 *
 * All these files are always marked as used in the bitmap attribute of the
 * mft; presumably in order to avoid accidental allocation for random other
 * mft records. Also, the sequence number for each of the system files is
 * always equal to their mft record number and it is never modified.
 */
typedef enum {
    FILE_MFT        = 0,                    /* Master file table (mft). Data attribute
                                            contains the entries and bitmap attribute
                                            records which ones are in use (bit==1). */
    FILE_MFTMirr    = 1,                    /* Mft mirror: copy of first four mft records
                                            in data attribute. If cluster size > 4kiB,
                                            copy of first N mft records, with
                                            N = cluster_size / mft_record_size. */
    FILE_LogFile    = 2,                    /* Journalling log in data attribute. */
    FILE_Volume     = 3,                    /* Volume name attribute and volume information
                                            attribute (flags and ntfs version). Windows
                                            refers to this file as volume DASD (Direct
                                            Access Storage Device). */
    FILE_AttrDef    = 4,                    /* Array of attribute definitions in data
                                            attribute. */
    FILE_root       = 5,                    /* Root directory. */
    FILE_Bitmap     = 6,                    /* Allocation bitmap of all clusters (lcns) in
                                            data attribute. */
    FILE_Boot       = 7,                    /* Boot sector (always at cluster 0) in data
                                            attribute. */
    FILE_BadClus    = 8,                    /* Contains all bad clusters in the non-resident
                                            data attribute. */
    FILE_Secure     = 9,                    /* Shared security descriptors in data attribute
                                            and two indexes into the descriptors.
                                            Appeared in Windows 2000. Before that, this
                                            file was named $Quota but was unused. */
    FILE_UpCase     = 10,                   /* Uppercase equivalents of all 65536 Unicode
                                            characters in data attribute. */
    FILE_Extend     = 11,                   /* Directory containing other system files (eg.
                                            $ObjId, $Quota, $Reparse and $UsnJrnl). This
                                            is new to NTFS3.0. */
    FILE_reserved12 = 12,                   /* Reserved for future use (records 12-15). */
    FILE_reserved13 = 13,
    FILE_reserved14 = 14,
    FILE_reserved15 = 15,
    FILE_first_user = 16,                   /* First user file, used as test limit for
                                            whether to allow opening a file or not. */
} NTFS_SYSTEM_FILES;

/**
 * enum MFT_RECORD_FLAGS -
 *
 * These are the so far known MFT_RECORD_* flags (16-bit) which contain
 * information about the mft record in which they are present.
 * 
 * MFT_RECORD_IS_4 exists on all $Extend sub-files.
 * It seems that it marks it is a metadata file with MFT record >24, however,
 * it is unknown if it is limited to metadata files only.
 *
 * MFT_RECORD_IS_VIEW_INDEX exists on every metafile with a non directory
 * index, that means an INDEX_ROOT and an INDEX_ALLOCATION with a name other
 * than "$I30". It is unknown if it is limited to metadata files only.
 */
typedef enum:WORD {
    MFT_RECORD_UNUSED           = 0x0000,
    MFT_RECORD_IN_USE           = 0x0001,
    MFT_RECORD_IS_DIRECTORY     = 0x0002,
    MFT_RECORD_IS_4             = 0x0004,
    MFT_RECORD_IS_VIEW_INDEX    = 0x0008,
    MFT_REC_SPACE_FILLER        = 0xffff,   /* Just to make flags 16-bit. */
}MFT_RECORD_FLAGS;

/*
 * mft references (aka file references or file record segment references) are
 * used whenever a structure needs to refer to a record in the mft.
 *
 * A reference consists of a 48-bit index into the mft and a 16-bit sequence
 * number used to detect stale references.
 *
 * For error reporting purposes we treat the 48-bit index as a signed quantity.
 *`
 * The sequence number is a circular counter (skipping 0) describing how many
 * times the referenced mft record has been (re)used. This has to match the
 * sequence number of the mft record being referenced, otherwise the reference
 * is considered stale and removed (FIXME: only ntfsck or the driver itself?).
 *
 * If the sequence number is zero it is assumed that no sequence number
 * consistency checking should be performed.
 *
 * FIXME: Since inodes are 32-bit as of now, the driver needs to always check
 * for high_part being 0 and if not either BUG(), cause a panic() or handle
 * the situation in some other way. This shouldn't be a problem as a volume has
 * to become HUGE in order to need more than 32-bits worth of mft records.
 * Assuming the standard mft record size of 1kb only the records (never mind
 * the non-resident attributes, etc.) would require 4Tb of space on their own
 * for the first 32 bits worth of records. This is only if some strange person
 * doesn't decide to foul play and make the mft sparse which would be a really
 * horrible thing to do as it would trash our current driver implementation. )-:
 * Do I hear screams "we want 64-bit inodes!" ?!? (-;
 *
 * FIXME: The mft zone is defined as the first 12% of the volume. This space is
 * reserved so that the mft can grow contiguously and hence doesn't become
 * fragmented. Volume free space includes the empty part of the mft zone and
 * when the volume's free 88% are used up, the mft zone is shrunk by a factor
 * of 2, thus making more space available for more files/data. This process is
 * repeated every time there is no more free space except for the mft zone until
 * there really is no more free space.
 */

/*
 * Typedef the MFT_REF as a 64-bit value for easier handling.
 * Also define two unpacking macros to get to the reference (MREF) and
 * sequence number (MSEQNO) respectively.
 * The _LE versions are to be applied on little endian MFT_REFs.
 * Note: The _LE versions will return a CPU endian formatted value!
 */
#define MFT_REF_MASK_CPU    0x0000ffffffffffffULL
#define MFT_REF_MASK_LE     MFT_REF_MASK_CPU

typedef DWORDLONG MFT_REF;
typedef DWORDLONG leMFT_REF;   /* a little-endian MFT_MREF */

#define MK_MREF(m, s)       ((MFT_REF)(((MFT_REF)(s) << 48) |        \
                            ((MFT_REF)(m) & MFT_REF_MASK_CPU)))
#define MK_LE_MREF(m, s)    ((MFT_REF)(((MFT_REF)(s) << 48) | \
                            ((MFT_REF)(m) & MFT_REF_MASK_CPU)))

#define MREF(x)             ((DWORDLONG)((x) & MFT_REF_MASK_CPU))
#define MSEQNO(x)           ((WORD)(((x) >> 48) & 0xffff))
#define MREF_LE(x)          ((DWORDLONG)((x) & MFT_REF_MASK_CPU))
#define MSEQNO_LE(x)        ((WORD)(((x) >> 48) & 0xffff))

#define IS_ERR_MREF(x)    (((x) & 0x0000800000000000ULL) ? 1 : 0)
#define ERR_MREF(x)    ((DWORDLONG)((DWORDLONG)(x)))
#define MREF_ERR(x)    ((int)((DWORDLONG)(x)))

/**
 * struct MFT_RECORD - An MFT record layout (NTFS 3.1+)
 *
 * The mft record header present at the beginning of every record in the mft.
 * This is followed by a sequence of variable length attribute records which
 * is terminated by an attribute of type AT_END which is a truncated attribute
 * in that it only consists of the attribute type code AT_END and none of the
 * other members of the attribute structure are present.
 */
typedef struct {
/*0x00*/NTFS_RECORD_TYPES magic;            /* Usually the magic is "FILE". */
/*0x04*/WORD usa_ofs;                       /* See NTFS_RECORD definition above. */
/*0x06*/WORD usa_count;                     /* See NTFS_RECORD definition above. */
/*0x08*/DWORDLONG lsn;                      /* $LogFile sequence number for this record.
                                            Changed every time the record is modified. */
/*0x10*/WORD sequence_number;               /* Number of times this mft record has been
                                            reused. (See description for MFT_REF
                                            above.) NOTE: The increment (skipping zero)
                                            is done when the file is deleted. NOTE: If
                                            this is zero it is left zero. */
/*0x12*/WORD link_count;                    /* Number of hard links, i.e. the number of
                                            directory entries referencing this record.
                                            NOTE: Only used in mft base records.
                                            NOTE: When deleting a directory entry we
                                            check the link_count and if it is 1 we
                                            delete the file. Otherwise we delete the
                                            FILE_NAME_ATTR being referenced by the
                                            directory entry from the mft record and
                                            decrement the link_count.
                                            FIXME: Careful with Win32 + DOS names! */
/*0x14*/WORD attrs_offset;                  /* Byte offset to the first attribute in this
                                            mft record from the start of the mft record.
                                            NOTE: Must be aligned to 8-byte boundary. */
/*0x16*/MFT_RECORD_FLAGS flags;             /* Bit array of MFT_RECORD_FLAGS. When a file
                                            is deleted, the MFT_RECORD_IN_USE flag is
                                            set to zero.
                                            MFT_RECORD_FLAGS*/
/*0x18*/DWORD bytes_in_use;                 /* Number of bytes used in this mft record.
                                            NOTE: Must be aligned to 8-byte boundary. */
/*0x1c*/DWORD bytes_allocated;              /* Number of bytes allocated for this mft
                                            record. This should be equal to the mft
                                            record size. */
/*0x20*/MFT_REF base_mft_record;            /* This is zero for base mft records.
                                            When it is not zero it is a mft reference
                                            pointing to the base mft record to which
                                            this record belongs (this is then used to
                                            locate the attribute list attribute present
                                            in the base record which describes this
                                            extension record and hence might need
                                            modification when the extension record
                                            itself is modified, also locating the
                                            attribute list also means finding the other
                                            potential extents, belonging to the non-base
                                            mft record). */
/*0x28*/WORD next_attr_instance;            /* The instance number that will be
                                            assigned to the next attribute added to this
                                            mft record. NOTE: Incremented each time
                                            after it is used. NOTE: Every time the mft
                                            record is reused this number is set to zero.
                                            NOTE: The first instance number is always 0.
                                            */
/* The below fields are specific to NTFS 3.1+ (Windows XP and above): */
/*0x2a*/ WORD reserved;                     /* Reserved/alignment. */
/*0x2c*/ DWORD mft_record_number;           /* Number of this mft record. */
/* sizeof() = 48 bytes */
/*
 * When (re)using the mft record, we place the update sequence array at this
 * offset, i.e. before we start with the attributes. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading we obviously use the data from the ntfs record header.
 */
} MFT_RECORD,*PMFT_RECORD;


/**
 * struct MFT_RECORD_OLD - An MFT record layout (NTFS <=3.0)
 *
 * This is the version without the NTFS 3.1+ specific fields.
 */
typedef struct {
/*0x00*/NTFS_RECORD_TYPES magic;/* Usually the magic is "FILE". */
/*0x04*/WORD usa_ofs;        /* See NTFS_RECORD definition above. */
/*0x06*/WORD usa_count;        /* See NTFS_RECORD definition above. */

/*0x08*/DWORDLONG lsn;        /* $LogFile sequence number for this record.
                   Changed every time the record is modified. */
/*0x10*/WORD sequence_number;    /* Number of times this mft record has been
                   reused. (See description for MFT_REF
                   above.) NOTE: The increment (skipping zero)
                   is done when the file is deleted. NOTE: If
                   this is zero it is left zero. */
/*0x12*/WORD link_count;        /* Number of hard links, i.e. the number of
                   directory entries referencing this record.
                   NOTE: Only used in mft base records.
                   NOTE: When deleting a directory entry we
                   check the link_count and if it is 1 we
                   delete the file. Otherwise we delete the
                   FILE_NAME_ATTR being referenced by the
                   directory entry from the mft record and
                   decrement the link_count.
                   FIXME: Careful with Win32 + DOS names! */
/*0x14*/WORD attrs_offset;    /* Byte offset to the first attribute in this
                   mft record from the start of the mft record.
                   NOTE: Must be aligned to 8-byte boundary. */
/*0x16*/WORD flags;    /* Bit array of MFT_RECORD_FLAGS. When a file
                   is deleted, the MFT_RECORD_IN_USE flag is
                   set to zero. 
                   MFT_RECORD_FLAGS*/
/*0x18*/DWORD bytes_in_use;    /* Number of bytes used in this mft record.
                   NOTE: Must be aligned to 8-byte boundary. */
/*0x1c*/DWORD bytes_allocated;    /* Number of bytes allocated for this mft
                   record. This should be equal to the mft
                   record size. */
/*0x20*/MFT_REF base_mft_record; /* This is zero for base mft records.
                   When it is not zero it is a mft reference
                   pointing to the base mft record to which
                   this record belongs (this is then used to
                   locate the attribute list attribute present
                   in the base record which describes this
                   extension record and hence might need
                   modification when the extension record
                   itself is modified, also locating the
                   attribute list also means finding the other
                   potential extents, belonging to the non-base
                   mft record). */
/*0x28*/WORD next_attr_instance; /* The instance number that will be
                   assigned to the next attribute added to this
                   mft record. NOTE: Incremented each time
                   after it is used. NOTE: Every time the mft
                   record is reused this number is set to zero.
                   NOTE: The first instance number is always 0.
                 */
/* sizeof() = 42 bytes */
/*
 * When (re)using the mft record, we place the update sequence array at this
 * offset, i.e. before we start with the attributes. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading we obviously use the data from the ntfs record header.
 */
} MFT_RECORD_OLD,*PMFT_RECORD_OLD;

/**
 * enum ATTR_TYPES - System defined attributes (32-bit).
 *
 * Each attribute type has a corresponding attribute name (Unicode string of
 * maximum 64 character length) as described by the attribute definitions
 * present in the data attribute of the $AttrDef system file.
 * 
 * On NTFS 3.0 volumes the names are just as the types are named in the below
 * enum exchanging AT_ for the dollar sign ($). If that isn't a revealing
 * choice of symbol... (-;
 */
typedef enum {
    AT_UNUSED               = 0x0, 
    AT_STANDARD_INFORMATION = 0x10,
    AT_ATTRIBUTE_LIST       = 0x20,
    AT_FILE_NAME            = 0x30,
    AT_OBJECT_ID            = 0x40,
    AT_SECURITY_DESCRIPTOR  = 0x50,
    AT_VOLUME_NAME          = 0x60,
    AT_VOLUME_INFORMATION   = 0x70,
    AT_DATA                 = 0x80,
    AT_INDEX_ROOT           = 0x90,
    AT_INDEX_ALLOCATION     = 0xa0,
    AT_BITMAP               = 0xb0,
    AT_REPARSE_POINT        = 0xc0,
    AT_EA_INFORMATION       = 0xd0,
    AT_EA                   = 0xe0,
    AT_PROPERTY_SET         = 0xf0,
    AT_LOGGED_UTILITY_STREAM = 0x100,
    AT_FIRST_USER_DEFINED_ATTRIBUTE = 0x1000,
    AT_END                  = 0xffffffff,
} ATTR_TYPES;

/**
 * enum COLLATION_RULES - The collation rules for sorting views/indexes/etc
 * (32-bit).
 *
 * COLLATION_UNICODE_STRING - Collate Unicode strings by comparing their binary
 *    Unicode values, except that when a character can be uppercased, the
 *    upper case value collates before the lower case one.
 * COLLATION_FILE_NAME - Collate file names as Unicode strings. The collation
 *    is done very much like COLLATION_UNICODE_STRING. In fact I have no idea
 *    what the difference is. Perhaps the difference is that file names
 *    would treat some special characters in an odd way (see
 *    unistr.c::ntfs_collate_names() and unistr.c::legal_ansi_char_array[]
 *    for what I mean but COLLATION_UNICODE_STRING would not give any special
 *    treatment to any characters at all, but this is speculation.
 * COLLATION_NTOFS_ULONG - Sorting is done according to ascending DWORD key
 *    values. E.g. used for $SII index in FILE_Secure, which sorts by
 *    security_id (DWORD).
 * COLLATION_NTOFS_SID - Sorting is done according to ascending SID values.
 *    E.g. used for $O index in FILE_Extend/$Quota.
 * COLLATION_NTOFS_SECURITY_HASH - Sorting is done first by ascending hash
 *    values and second by ascending security_id values. E.g. used for $SDH
 *    index in FILE_Secure.
 * COLLATION_NTOFS_ULONGS - Sorting is done according to a sequence of ascending
 *    DWORD key values. E.g. used for $O index in FILE_Extend/$ObjId, which
 *    sorts by object_id (16-byte), by splitting up the object_id in four
 *    DWORD values and using them as individual keys. E.g. take the following
 *    two security_ids, stored as follows on disk:
 *        1st: a1 61 65 b7 65 7b d4 11 9e 3d 00 e0 81 10 42 59
 *        2nd: 38 14 37 d2 d2 f3 d4 11 a5 21 c8 6b 79 b1 97 45
 *    To compare them, they are split into four DWORD values each, like so:
 *        1st: 0xb76561a1 0x11d47b65 0xe0003d9e 0x59421081
 *        2nd: 0xd2371438 0x11d4f3d2 0x6bc821a5 0x4597b179
 *    Now, it is apparent why the 2nd object_id collates after the 1st: the
 *    first DWORD value of the 1st object_id is less than the first DWORD of
 *    the 2nd object_id. If the first DWORD values of both object_ids were
 *    equal then the second DWORD values would be compared, etc.
 */
typedef enum {
    COLLATION_BINARY        = 0,            /* Collate by binary
                                            compare where the first byte is most
                                            significant. */
    COLLATION_FILE_NAME     = 1,            /* Collate file names
                                            as Unicode strings. */
    COLLATION_UNICODE_STRING= 2,            /* Collate Unicode
                                            strings by comparing their binary
                                            Unicode values, except that when a
                                            character can be uppercased, the upper
                                            case value collates before the lower
                                            case one. */
    COLLATION_NTOFS_ULONG   = 16,
    COLLATION_NTOFS_SID     = 17,
    COLLATION_NTOFS_SECURITY_HASH = 18,
    COLLATION_NTOFS_ULONGS  = 19,
} COLLATION_RULES;

/**
 * enum ATTR_DEF_FLAGS -
 *
 * The flags (32-bit) describing attribute properties in the attribute
 * definition structure.  FIXME: This information is based on Regis's
 * information and, according to him, it is not certain and probably
 * incomplete.  The INDEXABLE flag is fairly certainly correct as only the file
 * name attribute has this flag set and this is the only attribute indexed in
 * NT4.
 */
typedef enum {
    ATTR_DEF_INDEXABLE      = 0x02,         /* Attribute can be
                                            indexed. */
    ATTR_DEF_MULTIPLE       = 0x04,         /* Attribute type
                                            can be present multiple times in the
                                            mft records of an inode. */
    ATTR_DEF_NOT_ZERO       = 0x08,         /* Attribute value
                                            must contain at least one non-zero
                                            byte. */
    ATTR_DEF_INDEXED_UNIQUE = 0x10,         /* Attribute must be
                                            indexed and the attribute value must be
                                            unique for the attribute type in all of
                                            the mft records of an inode. */
    ATTR_DEF_NAMED_UNIQUE   = 0x20,         /* Attribute must be
                                            named and the name must be unique for
                                            the attribute type in all of the mft
                                            records of an inode. */
    ATTR_DEF_RESIDENT       = 0x40,         /* Attribute must be
                    resident. */
    ATTR_DEF_ALWAYS_LOG     = 0x80,         /* Always log
                                            modifications to this attribute,
                                            regardless of whether it is resident or
                                            non-resident.  Without this, only log
                                            modifications if the attribute is
                                            resident. */
} ATTR_DEF_FLAGS;

/**
 * struct ATTR_DEF -
 *
 * The data attribute of FILE_AttrDef contains a sequence of attribute
 * definitions for the NTFS volume. With this, it is supposed to be safe for an
 * older NTFS driver to mount a volume containing a newer NTFS version without
 * damaging it (that's the theory. In practice it's: not damaging it too much).
 * Entries are sorted by attribute type. The flags describe whether the
 * attribute can be resident/non-resident and possibly other things, but the
 * actual bits are unknown.
 */
typedef struct {
/*hex ofs*/
/*0x000*/WCHAR name[0x40];                  /* Unicode name of the attribute. Zero
                                            terminated. */
/*0x80*/ATTR_TYPES type;                    /* Type of the attribute. */
/*0x84*/DWORD display_rule;                 /* Default display rule.
                                            FIXME: What does it mean? (AIA) */
/*0x88*/COLLATION_RULES collation_rule;     /* Default collation rule. */
/*0x8c*/ATTR_DEF_FLAGS flags;               /* Flags describing the attribute. */
/*0x90*/DWORDLONG min_size;                 /* Optional minimum attribute size. */
/*0x98*/DWORDLONG max_size;                 /* Maximum size of attribute. */
/* sizeof() = 0xa0 or 160 bytes */
}ATTR_DEF;

/**
 * enum ATTR_FLAGS - Attribute flags (16-bit).
 */
typedef enum :WORD{
    ATTR_NORMAL             = 0x0000,
    ATTR_IS_COMPRESSED      = 0x0001,
    ATTR_COMPRESSION_MASK   = 0x00ff,       /* Compression
                                            method mask. Also, first
                                            illegal value. */
    ATTR_IS_ENCRYPTED       = 0x4000,
    ATTR_IS_SPARSE          = 0x8000,
} ATTR_FLAGS;

/*
 * Attribute compression.
 *
 * Only the data attribute is ever compressed in the current ntfs driver in
 * Windows. Further, compression is only applied when the data attribute is
 * non-resident. Finally, to use compression, the maximum allowed cluster size
 * on a volume is 4kib.
 *
 * The compression method is based on independently compressing blocks of X
 * clusters, where X is determined from the compression_unit value found in the
 * non-resident attribute record header (more precisely: X = 2^compression_unit
 * clusters). On Windows NT/2k, X always is 16 clusters (compression_unit = 4).
 *
 * There are three different cases of how a compression block of X clusters
 * can be stored:
 *
 *   1) The data in the block is all zero (a sparse block):
 *      This is stored as a sparse block in the runlist, i.e. the runlist
 *      entry has length = X and lcn = -1. The mapping pairs array actually
 *      uses a delta_lcn value length of 0, i.e. delta_lcn is not present at
 *      all, which is then interpreted by the driver as lcn = -1.
 *      NOTE: Even uncompressed files can be sparse on NTFS 3.0 volumes, then
 *      the same principles apply as above, except that the length is not
 *      restricted to being any particular value.
 *
 *   2) The data in the block is not compressed:
 *      This happens when compression doesn't reduce the size of the block
 *      in clusters. I.e. if compression has a small effect so that the
 *      compressed data still occupies X clusters, then the uncompressed data
 *      is stored in the block.
 *      This case is recognised by the fact that the runlist entry has
 *      length = X and lcn >= 0. The mapping pairs array stores this as
 *      normal with a run length of X and some specific delta_lcn, i.e.
 *      delta_lcn has to be present.
 *
 *   3) The data in the block is compressed:
 *      The common case. This case is recognised by the fact that the run
 *      list entry has length L < X and lcn >= 0. The mapping pairs array
 *      stores this as normal with a run length of X and some specific
 *      delta_lcn, i.e. delta_lcn has to be present. This runlist entry is
 *      immediately followed by a sparse entry with length = X - L and
 *      lcn = -1. The latter entry is to make up the vcn counting to the
 *      full compression block size X.
 *
 * In fact, life is more complicated because adjacent entries of the same type
 * can be coalesced. This means that one has to keep track of the number of
 * clusters handled and work on a basis of X clusters at a time being one
 * block. An example: if length L > X this means that this particular runlist
 * entry contains a block of length X and part of one or more blocks of length
 * L - X. Another example: if length L < X, this does not necessarily mean that
 * the block is compressed as it might be that the lcn changes inside the block
 * and hence the following runlist entry describes the continuation of the
 * potentially compressed block. The block would be compressed if the
 * following runlist entry describes at least X - L sparse clusters, thus
 * making up the compression block length as described in point 3 above. (Of
 * course, there can be several runlist entries with small lengths so that the
 * sparse entry does not follow the first data containing entry with
 * length < X.)
 *
 * NOTE: At the end of the compressed attribute value, there most likely is not
 * just the right amount of data to make up a compression block, thus this data
 * is not even attempted to be compressed. It is just stored as is, unless
 * the number of clusters it occupies is reduced when compressed in which case
 * it is stored as a compressed compression block, complete with sparse
 * clusters at the end.
 */

/**
 * enum RESIDENT_ATTR_FLAGS - Flags of resident attributes (8-bit).
 */
typedef enum:BYTE {
    RESIDENT_ATTR_IS_INDEXED = 0x01,        /* Attribute is referenced in an index
                                            (has implications for deleting and
                                            modifying the attribute). */
} RESIDENT_ATTR_FLAGS;

/**
 * struct ATTR_RECORD - Attribute record header.
 *
 * Always aligned to 8-byte boundary.
 */
typedef struct {
/*Ofs*/
/*0x00*/ATTR_TYPES type;                    /* The (32-bit) type of the attribute. */
/*0x04*/DWORD length;                       /* Byte size of the resident part of the
                                            attribute (aligned to 8-byte boundary).
                                            Used to get to the next attribute. */
/*0x08*/BYTE non_resident;                  /* If 0, attribute is resident.
                   If 1,                    attribute is non-resident. */
/*0x09*/BYTE name_length;                   /* Unicode character size of name of attribute.
                                            0 if unnamed. */
/*0x0a*/WORD name_offset;                   /* If name_length != 0, the byte offset to the
                                            beginning of the name from the attribute
                                            record. Note that the name is stored as a
                                            Unicode string. When creating, place offset
                                            just at the end of the record header. Then,
                                            follow with attribute value or mapping pairs
                                            array, resident and non-resident attributes
                                            respectively, aligning to an 8-byte
                                                boundary. */
/*0x0c*/ATTR_FLAGS flags;                   /* Flags describing the attribute.ATTR_FLAGS */
/*0x0e*/WORD instance;                      /* The instance of this attribute record. This
                                            number is unique within this mft record (see
                                            MFT_RECORD/next_attribute_instance notes
                                            above for more details). */
        union {
            /* Resident attributes. */
            struct {
/*0x10*/        DWORD value_length;         /* Byte size of attribute value. */
/*0x14*/        WORD value_offset;          /* Byte offset of the attribute
                                            value from the start of the
                                            attribute record. When creating,
                                            align to 8-byte boundary if we
                                            have a name present as this might
                                            not have a length of a multiple
                                            of 8-bytes. */
/*0x16*/        RESIDENT_ATTR_FLAGS resident_flags; /* See above RESIDENT_ATTR_FLAGS. */
/*0x17*/        BYTE reserved;              /* Reserved/alignment to 8-byte
                                            boundary.*/
/*0x18*/        void *resident_end[0];      /* Use offsetof(ATTR_RECORD,
                                            resident_end) to get size of
                                            a resident attribute. */
            };
            /* Non-resident attributes. */
            struct {
/*0x10*/        DWORDLONG lowest_vcn;       /* Lowest valid virtual cluster number
                                            for this portion of the attribute value or
                                            0 if this is the only extent (usually the
                                            case). - Only when an attribute list is used
                                            does lowest_vcn != 0 ever occur. */
/*0x18*/        DWORDLONG highest_vcn;      /* Highest valid vcn of this extent of
                                            the attribute value. - Usually there is only one
                                            portion, so this usually equals the attribute
                                            value size in clusters minus 1. Can be -1 for
                                            zero length files. Can be 0 for "single extent"
                                            attributes. */
/*0x20*/        WORD mapping_pairs_offset; /* Byte offset from the
                                            beginning of the structure to the mapping pairs
                                            array which contains the mappings between the
                                            vcns and the logical cluster numbers (lcns).
                                            When creating, place this at the end of this
                                            record header aligned to 8-byte boundary. */
/*0x22*/        BYTE compression_unit;      /* The compression unit expressed
                                            as the log to the base 2 of the number of
                                            clusters in a compression unit. 0 means not
                                            compressed. (This effectively limits the
                                            compression unit size to be a power of two
                                            clusters.) WinNT4 only uses a value of 4. */
/*0x23*/        BYTE reserved1[5];          /* Align to 8-byte boundary. */
                                            /* The sizes below are only used when lowest_vcn is zero, as otherwise it would
                                            be difficult to keep them up-to-date.*/
/*0x28*/        DWORDLONG allocated_size;   /* Byte size of disk space
                                            allocated to hold the attribute value. Always
                                            is a multiple of the cluster size. When a file
                                            is compressed, this field is a multiple of the
                                            compression block size (2^compression_unit) and
                                            it represents the logically allocated space
                                            rather than the actual on disk usage. For this
                                            use the compressed_size (see below). */
/* 0x30*/        DWORDLONG data_size;       /* Byte size of the attribute
                                            value. Can be larger than allocated_size if
                                            attribute value is compressed or sparse. */
/* 0x38*/        DWORDLONG initialized_size;/* Byte size of initialized
                                            portion of the attribute value. Usually equals
                                            data_size. */
/*0x40 */       void *non_resident_end[0]; /* Use offsetof(ATTR_RECORD,
                                            non_resident_end) to get
                                            size of a non resident
                                            attribute. */
                /* sizeof(uncompressed attr) = 64*/
/*0x40*/        /*DWORDLONG compressed_size;*/  /* Byte size of the attribute
                                            value after compression. Only present when
                                            compressed. Always is a multiple of the
                                            cluster size. Represents the actual amount of
                                            disk space being used on the disk. */
/*0x48*/        /*void *compressed_end[0];*/    /* Use offsetof(ATTR_RECORD, compressed_end) to
                                            get size of a compressed attribute. */
/* sizeof(compressed attr) = 72*/
            };
    };
} ATTR_RECORD,*PATTR_RECORD;

typedef ATTR_RECORD ATTR_REC;
typedef PATTR_RECORD PATTR_REC;


/**
 * enum FILE_ATTR_FLAGS - File attribute flags (32-bit).
 */
typedef enum {
    /*
     * These flags are only present in the STANDARD_INFORMATION attribute
     * (in the field file_attributes).
     */
    FILE_ATTR_READONLY          = 0x00000001,
    FILE_ATTR_HIDDEN            = 0x00000002,
    FILE_ATTR_SYSTEM            = 0x00000004,
    /* Old DOS volid. Unused in NT.    = cpu_to_le32(0x00000008), */

    FILE_ATTR_DIRECTORY         = 0x00000010,/* FILE_ATTR_DIRECTORY is not considered valid in NT. It is reserved
                                            for the DOS SUBDIRECTORY flag. */
    FILE_ATTR_ARCHIVE           = 0x00000020,
    FILE_ATTR_DEVICE            = 0x00000040,
    FILE_ATTR_NORMAL            = 0x00000080,

    FILE_ATTR_TEMPORARY         = 0x00000100,
    FILE_ATTR_SPARSE_FILE       = 0x00000200,
    FILE_ATTR_REPARSE_POINT     = 0x00000400,
    FILE_ATTR_COMPRESSED        = 0x00000800,

    FILE_ATTR_OFFLINE           = 0x00001000,
    FILE_ATTR_NOT_CONTENT_INDEXED  = 0x00002000,
    FILE_ATTR_ENCRYPTED         = 0x00004000,

    FILE_ATTR_VALID_FLAGS       = 0x00007fb7,/* FILE_ATTR_VALID_FLAGS masks out the old DOS VolId and the
                                            FILE_ATTR_DEVICE and preserves everything else. This mask
                                            is used to obtain all flags that are valid for reading. */
    FILE_ATTR_VALID_SET_FLAGS   = 0x000031a7,/* FILE_ATTR_VALID_SET_FLAGS masks out the old DOS VolId, the
                                            FILE_ATTR_DEVICE, FILE_ATTR_DIRECTORY, FILE_ATTR_SPARSE_FILE,
                                            FILE_ATTR_REPARSE_POINT, FILE_ATRE_COMPRESSED and FILE_ATTR_ENCRYPTED
                                            and preserves the rest. This mask is used to to obtain all flags that
                                            are valid for setting. */

    /**
     * FILE_ATTR_I30_INDEX_PRESENT - Is it a directory?
     *
     * This is a copy of the MFT_RECORD_IS_DIRECTORY bit from the mft
     * record, telling us whether this is a directory or not, i.e. whether
     * it has an index root attribute named "$I30" or not.
     * 
     * This flag is only present in the FILE_NAME attribute (in the 
     * file_attributes field).
     */
    FILE_ATTR_I30_INDEX_PRESENT    = 0x10000000,
    
    /**
     * FILE_ATTR_VIEW_INDEX_PRESENT - Does have a non-directory index?
     * 
     * This is a copy of the MFT_RECORD_IS_VIEW_INDEX bit from the mft
     * record, telling us whether this file has a view index present (eg.
     * object id index, quota index, one of the security indexes and the
     * reparse points index).
     *
     * This flag is only present in the $STANDARD_INFORMATION and
     * $FILE_NAME attributes.
     */
    FILE_ATTR_VIEW_INDEX_PRESENT    = 0x20000000,
} FILE_ATTR_FLAGS;

/*
 * NOTE on times in NTFS: All times are in MS standard time format, i.e. they
 * are the number of 100-nanosecond intervals since 1st January 1601, 00:00:00
 * universal coordinated time (UTC). (In Linux time starts 1st January 1970,
 * 00:00:00 UTC and is stored as the number of 1-second intervals since then.)
 */

/**
 * struct STANDARD_INFORMATION - Attribute: Standard information (0x10).
 *
 * NOTE: Always resident.
 * NOTE: Present in all base file records on a volume.
 * NOTE: There is conflicting information about the meaning of each of the time
 *     fields but the meaning as defined below has been verified to be
 *     correct by practical experimentation on Windows NT4 SP6a and is hence
 *     assumed to be the one and only correct interpretation.
 */
typedef struct {

/*0x00*/DWORDLONG creation_time;            /* Time file was created. Updated when
                                            a filename is changed(?). */
/*0x08*/DWORDLONG last_data_change_time;    /* Time the data attribute was last
                                            modified. */
/*0x10*/DWORDLONG last_mft_change_time;     /* Time this mft record was last
                                            modified. */
/*0x18*/DWORDLONG last_access_time;         /* Approximate time when the file was
                                            last accessed (obviously this is not
                                            updated on read-only volumes). In
                                            Windows this is only updated when
                                            accessed if some time delta has
                                            passed since the last update. Also,
                                            last access times updates can be
                                            disabled altogether for speed. */
/*0x20*/FILE_ATTR_FLAGS file_attributes;    /* Flags describing the file. */
/*0x24*/union {
            /* NTFS 1.2 (and previous, presumably) */
            struct {
/*0x24*/         BYTE reserved12[12];       /* Reserved/alignment to 8-byte
                                            boundary. */
/*0x30*/         void *v1_end[0];           /* Marker for offsetof(). */
            };
            /* sizeof() = 48 bytes */
            /* NTFS 3.0 */
            struct {
/*
 * If a volume has been upgraded from a previous NTFS version, then these
 * fields are present only if the file has been accessed since the upgrade.
 * Recognize the difference by comparing the length of the resident attribute
 * value. If it is 48, then the following fields are missing. If it is 72 then
 * the fields are present. Maybe just check like this:
 *    if (resident.ValueLength < sizeof(STANDARD_INFORMATION)) {
 *        Assume NTFS 1.2- format.
 *        If (volume version is 3.0+)
 *            Upgrade attribute to NTFS 3.0 format.
 *        else
 *            Use NTFS 1.2- format for access.
 *    } else
 *        Use NTFS 3.0 format for access.
 * Only problem is that it might be legal to set the length of the value to
 * arbitrarily large values thus spoiling this check. - But chkdsk probably
 * views that as a corruption, assuming that it behaves like this for all
 * attributes.
 */
/*0x24*/       DWORD maximum_versions;      /* Maximum allowed versions for
                                            file. Zero if version numbering is disabled. */
/*0x28*/       DWORD version_number;        /* This file's version (if any).
                                            Set to zero if maximum_versions is zero. */
/*0x2c*/       DWORD class_id;              /* Class id from bidirectional
                                            class id index (?). */
/*0x30*/       DWORD owner_id;              /* Owner_id of the user owning
                                            the file. Translate via $Q index in FILE_Extend
                                            /$Quota to the quota control entry for the user
                                            owning the file. Zero if quotas are disabled. */
/*0x34*/       DWORD security_id;           /* Security_id for the file.
                                            Translate via $SII index and $SDS data stream
                                            in FILE_Secure to the security descriptor. */
/*0x38*/       DWORDLONG quota_charged;     /* Byte size of the charge to
                                            the quota for all streams of the file. Note: Is
                                            zero if quotas are disabled. */
/*0x40*/       DWORDLONG usn;               /* Last update sequence number
                                            of the file. This is a direct index into the
                                            change (aka usn) journal file. It is zero if
                                            the usn journal is disabled.
                                            NOTE: To disable the journal need to delete
                                            the journal file itself and to then walk the
                                            whole mft and set all Usn entries in all mft
                                            records to zero! (This can take a while!)
                                            The journal is FILE_Extend/$UsnJrnl. Win2k
                                            will recreate the journal and initiate
                                            logging if necessary when mounting the
                                            partition. This, in contrast to disabling the
                                            journal is a very fast process, so the user
                                            won't even notice it. */
/*0x48*/        void *v3_end[0];            /* Marker for offsetof(). */
            };
        };
/* sizeof() = 72 bytes (NTFS 3.0) */
} STANDARD_INFORMATION,*PSTANDARD_INFORMATION;


/**
 * struct ATTR_LIST_ENTRY - Attribute: Attribute list (0x20).
 *
 * - Can be either resident or non-resident.
 * - Value consists of a sequence of variable length, 8-byte aligned,
 * ATTR_LIST_ENTRY records.
 * - The attribute list attribute contains one entry for each attribute of
 * the file in which the list is located, except for the list attribute
 * itself. The list is sorted: first by attribute type, second by attribute
 * name (if present), third by instance number. The extents of one
 * non-resident attribute (if present) immediately follow after the initial
 * extent. They are ordered by lowest_vcn and have their instance set to zero.
 * It is not allowed to have two attributes with all sorting keys equal.
 * - Further restrictions:
 *    - If not resident, the vcn to lcn mapping array has to fit inside the
 *      base mft record.
 *    - The attribute list attribute value has a maximum size of 256kb. This
 *      is imposed by the Windows cache manager.
 * - Attribute lists are only used when the attributes of mft record do not
 * fit inside the mft record despite all attributes (that can be made
 * non-resident) having been made non-resident. This can happen e.g. when:
 *    - File has a large number of hard links (lots of file name
 *      attributes present).
 *    - The mapping pairs array of some non-resident attribute becomes so
 *      large due to fragmentation that it overflows the mft record.
 *    - The security descriptor is very complex (not applicable to
 *      NTFS 3.0 volumes).
 *    - There are many named streams.
 */
typedef struct {
/*Ofs*/
/*0x00*/    ATTR_TYPES type;                /* Type of referenced attribute. */
/*0x04*/    WORD length;                    /* Byte size of this entry. */
/*0x06*/    BYTE name_length;               /* Size in Unicode chars of the name of the
                                            attribute or 0 if unnamed. */
/*0x07*/    BYTE name_offset;               /* Byte offset to beginning of attribute name
                                            (always set this to where the name would
                                            start even if unnamed). */
/*0x08*/    LONGLONG lowest_vcn;           /* Lowest virtual cluster number of this portion
                                            of the attribute value. This is usually 0. It
                                            is non-zero for the case where one attribute
                                            does not fit into one mft record and thus
                                            several mft records are allocated to hold
                                            this attribute. In the latter case, each mft
                                            record holds one extent of the attribute and
                                            there is one attribute list entry for each
                                            extent. NOTE: This is DEFINITELY a signed
                                            value! The windows driver uses cmp, followed
                                            by jg when comparing this, thus it treats it
                                            as signed. */
/*0x10*/    MFT_REF mft_reference;          /* The reference of the mft record holding
                                            the ATTR_RECORD for this portion of the
                                            attribute value. */
/*0x18*/    WORD instance;                  /* If lowest_vcn = 0, the instance of the
                                            attribute being referenced; otherwise 0. */
/*0x1a*/    WCHAR name[0];                  /* Use when creating only. When reading use
                                            name_offset to determine the location of the
                                            name. */
/* sizeof() = 26 + (attribute_name_length * 2) bytes */
}ATTR_LIST_ENTRY,*PATTR_LIST_ENTRY;

/*
 * The maximum allowed length for a file name.
 */
#define NTFS_MAX_NAME_LEN    255

/**
 * enum FILE_NAME_TYPE_FLAGS - Possible namespaces for filenames in ntfs.
 * (8-bit).
 */
typedef enum :BYTE{
    FILE_NAME_POSIX         = 0x00,         /* This is the largest namespace. It is case sensitive and
                                            allows all Unicode characters except for: '\0' and '/'.
                                            Beware that in WinNT/2k files which eg have the same name
                                            except for their case will not be distinguished by the
                                            standard utilities and thus a "del filename" will delete
                                            both "filename" and "fileName" without warning. */
    FILE_NAME_WIN32         = 0x01,         /* The standard WinNT/2k NTFS long filenames. Case insensitive.
                                            All Unicode chars except: '\0', '"', '*', '/', ':', '<',
                                            '>', '?', '\' and '|'. Further, names cannot end with a '.'
                                            or a space. */
    FILE_NAME_DOS           = 0x02,        /* The standard DOS filenames (8.3 format). Uppercase only.
                                            All 8-bit characters greater space, except: '"', '*', '+',
                                            ',', '/', ':', ';', '<', '=', '>', '?' and '\'. */
    FILE_NAME_WIN32_AND_DOS = 0x03,         /* 3 means that both the Win32 and the DOS filenames are
                                            identical and hence have been saved in this single filename
                                            record. */
} FILE_NAME_TYPE_FLAGS;

/**
 * struct FILE_NAME_ATTR - Attribute: Filename (0x30).
 *
 * NOTE: Always resident.
 * NOTE: All fields, except the parent_directory, are only updated when the
 *     filename is changed. Until then, they just become out of sync with
 *     reality and the more up to date values are present in the standard
 *     information attribute.
 * NOTE: There is conflicting information about the meaning of each of the time
 *     fields but the meaning as defined below has been verified to be
 *     correct by practical experimentation on Windows NT4 SP6a and is hence
 *     assumed to be the one and only correct interpretation.
 */
typedef struct {

/*0x00*/MFT_REF parent_directory;           /* Directory this filename is
                                            referenced from. */
/*0x08*/DWORDLONG creation_time;            /* Time file was created. */
/*0x10*/DWORDLONG last_data_change_time;    /* Time the data attribute was last
                                            modified. */
/*0x18*/DWORDLONG last_mft_change_time;     /* Time this mft record was last
                                            modified. */
/*0x20*/DWORDLONG last_access_time;         /* Last time this mft record was
                                            accessed. */
/*0x28*/DWORDLONG allocated_size;           /* Byte size of on-disk allocated space
                                            for the data attribute.  So for
                                            normal $DATA, this is the
                                            allocated_size from the unnamed
                                            $DATA attribute and for compressed
                                            and/or sparse $DATA, this is the
                                            compressed_size from the unnamed
                                            $DATA attribute.  NOTE: This is a
                                            multiple of the cluster size. */
/*0x30*/DWORDLONG data_size;                /* Byte size of actual data in data
                                            attribute. */
/*0x38*/FILE_ATTR_FLAGS file_attributes;    /* Flags describing the file. */
/*0x3c*/union {
            struct {
/*0x3c*/       WORD packed_ea_size;         /* Size of the buffer needed to
                                            pack the extended attributes
                                            (EAs), if such are present.*/
/*0x3e*/       WORD reserved;               /* Reserved for alignment. */
            };
/*0x3c*/    DWORD reparse_point_tag;        /* Type of reparse point,
                                            present only in reparse
                                            points and only if there are
                                            no EAs. */
        };
/*0x40*/BYTE file_name_length;              /* Length of file name in
                                            (Unicode) characters. */
/*0x41*/FILE_NAME_TYPE_FLAGS file_name_type;/* Namespace of the file name.*/
/*0x42*/WCHAR file_name[0];                 /* File name in Unicode. */
} FILE_NAME_ATTR,*PFILE_NAME_ATTR;


/**
 * struct OBJ_ID_INDEX_DATA - FILE_Extend/$ObjId contains an index named $O.
 *
 * This index contains all object_ids present on the volume as the index keys
 * and the corresponding mft_record numbers as the index entry data parts.
 *
 * The data part (defined below) also contains three other object_ids:
 *    birth_volume_id - object_id of FILE_Volume on which the file was first
 *              created. Optional (i.e. can be zero).
 *    birth_object_id - object_id of file when it was first created. Usually
 *              equals the object_id. Optional (i.e. can be zero).
 *    domain_id    - Reserved (always zero).
 */
typedef struct {
    MFT_REF mft_reference;                  /* Mft record containing the object_id in
                                            the index entry key. */
    union {
        struct {
            GUID birth_volume_id;
            GUID birth_object_id;
            GUID domain_id;
        };
        BYTE extended_info[48];
    };
} OBJ_ID_INDEX_DATA,*POBJ_ID_INDEX_DATA;

/**
 * struct OBJECT_ID_ATTR - Attribute: Object id (NTFS 3.0+) (0x40).
 *
 * NOTE: Always resident.
 */
typedef struct {
    GUID object_id;                         /* Unique id assigned to the
                                            file.*/
    /* The following fields are optional. The attribute value size is 16
       bytes, i.e. sizeof(GUID), if these are not present at all. Note,
       the entries can be present but one or more (or all) can be zero
       meaning that that particular value(s) is(are) not defined. Note,
       when the fields are missing here, it is well possible that they are
       to be found within the $Extend/$ObjId system file indexed under the
       above object_id. */
    union {
        struct {
            GUID birth_volume_id;           /* Unique id of volume on which
                                            the file was first created.*/
            GUID birth_object_id;           /* Unique id of file when it was
                                            first created. */
            GUID domain_id;                 /* Reserved, zero. */
        };
        BYTE extended_info[48];
    };
}OBJECT_ID_ATTR,*POBJECT_ID_ATTR;

#if 0
/**
 * enum IDENTIFIER_AUTHORITIES -
 *
 * The pre-defined IDENTIFIER_AUTHORITIES used as SID_IDENTIFIER_AUTHORITY in
 * the SID structure (see below).
 */
typedef enum {                    /* SID string prefix. */
    SECURITY_NULL_SID_AUTHORITY    = {0, 0, 0, 0, 0, 0},    /* S-1-0 */
    SECURITY_WORLD_SID_AUTHORITY    = {0, 0, 0, 0, 0, 1},    /* S-1-1 */
    SECURITY_LOCAL_SID_AUTHORITY    = {0, 0, 0, 0, 0, 2},    /* S-1-2 */
    SECURITY_CREATOR_SID_AUTHORITY    = {0, 0, 0, 0, 0, 3},    /* S-1-3 */
    SECURITY_NON_UNIQUE_AUTHORITY    = {0, 0, 0, 0, 0, 4},    /* S-1-4 */
    SECURITY_NT_SID_AUTHORITY    = {0, 0, 0, 0, 0, 5},    /* S-1-5 */
} IDENTIFIER_AUTHORITIES;
#endif

/**
 * enum RELATIVE_IDENTIFIERS -
 *
 * These relative identifiers (RIDs) are used with the above identifier
 * authorities to make up universal well-known SIDs.
 *
 * Note: The relative identifier (RID) refers to the portion of a SID, which
 * identifies a user or group in relation to the authority that issued the SID.
 * For example, the universal well-known SID Creator Owner ID (S-1-3-0) is
 * made up of the identifier authority SECURITY_CREATOR_SID_AUTHORITY (3) and
 * the relative identifier SECURITY_CREATOR_OWNER_RID (0).
 */
#if 0
typedef enum {                              /* Identifier authority. */
    SECURITY_NULL_RID           = 0,        /* S-1-0 */
    SECURITY_WORLD_RID          = 0,        /* S-1-1 */
    SECURITY_LOCAL_RID          = 0,        /* S-1-2 */

    SECURITY_CREATOR_OWNER_RID  = 0,        /* S-1-3 */
    SECURITY_CREATOR_GROUP_RID  = 1,        /* S-1-3 */

    SECURITY_CREATOR_OWNER_SERVER_RID = 2,  /* S-1-3 */
    SECURITY_CREATOR_GROUP_SERVER_RID = 3,  /* S-1-3 */

    SECURITY_DIALUP_RID         = 1,
    SECURITY_NETWORK_RID        = 2,
    SECURITY_BATCH_RID          = 3,
    SECURITY_INTERACTIVE_RID    = 4,
    SECURITY_SERVICE_RID        = 6,
    SECURITY_ANONYMOUS_LOGON_RID = 7,
    SECURITY_PROXY_RID          = 8,
    SECURITY_ENTERPRISE_CONTROLLERS_RID=9,
    SECURITY_SERVER_LOGON_RID   = 9,
    SECURITY_PRINCIPAL_SELF_RID = 0xa,
    SECURITY_AUTHENTICATED_USER_RID   = 0xb,
    SECURITY_RESTRICTED_CODE_RID      = 0xc,
    SECURITY_TERMINAL_SERVER_RID      = 0xd,

    SECURITY_LOGON_IDS_RID            = 5,
    SECURITY_LOGON_IDS_RID_COUNT      = 3,

    SECURITY_LOCAL_SYSTEM_RID         = 0x12,

    SECURITY_NT_NON_UNIQUE            = 0x15,

    SECURITY_BUILTIN_DOMAIN_RID       = 0x20,

    /*
     * Well-known domain relative sub-authority values (RIDs).
     */

    /* Users. */
    DOMAIN_USER_RID_ADMIN           = 0x1f4,
    DOMAIN_USER_RID_GUEST           = 0x1f5,
    DOMAIN_USER_RID_KRBTGT          = 0x1f6,

    /* Groups. */
    DOMAIN_GROUP_RID_ADMINS         = 0x200,
    DOMAIN_GROUP_RID_USERS          = 0x201,
    DOMAIN_GROUP_RID_GUESTS         = 0x202,
    DOMAIN_GROUP_RID_COMPUTERS      = 0x203,
    DOMAIN_GROUP_RID_CONTROLLERS    = 0x204,
    DOMAIN_GROUP_RID_CERT_ADMINS    = 0x205,
    DOMAIN_GROUP_RID_SCHEMA_ADMINS  = 0x206,
    DOMAIN_GROUP_RID_ENTERPRISE_ADMINS= 0x207,
    DOMAIN_GROUP_RID_POLICY_ADMINS  = 0x208,

    /* Aliases. */
    DOMAIN_ALIAS_RID_ADMINS         = 0x220,
    DOMAIN_ALIAS_RID_USERS          = 0x221,
    DOMAIN_ALIAS_RID_GUESTS         = 0x222,
    DOMAIN_ALIAS_RID_POWER_USERS    = 0x223,

    DOMAIN_ALIAS_RID_ACCOUNT_OPS    = 0x224,
    DOMAIN_ALIAS_RID_SYSTEM_OPS     = 0x225,
    DOMAIN_ALIAS_RID_PRINT_OPS      = 0x226,
    DOMAIN_ALIAS_RID_BACKUP_OPS     = 0x227,

    DOMAIN_ALIAS_RID_REPLICATOR     = 0x228,
    DOMAIN_ALIAS_RID_RAS_SERVERS    = 0x229,
    DOMAIN_ALIAS_RID_PREW2KCOMPACCESS = 0x22a,
} RELATIVE_IDENTIFIERS;
#endif

/*
 * The universal well-known SIDs:
 *
 *    NULL_SID            S-1-0-0
 *    WORLD_SID            S-1-1-0
 *    LOCAL_SID            S-1-2-0
 *    CREATOR_OWNER_SID        S-1-3-0
 *    CREATOR_GROUP_SID        S-1-3-1
 *    CREATOR_OWNER_SERVER_SID    S-1-3-2
 *    CREATOR_GROUP_SERVER_SID    S-1-3-3
 *
 *    (Non-unique IDs)        S-1-4
 *
 * NT well-known SIDs:
 *
 *    NT_AUTHORITY_SID    S-1-5
 *    DIALUP_SID        S-1-5-1
 *
 *    NETWORD_SID        S-1-5-2
 *    BATCH_SID        S-1-5-3
 *    INTERACTIVE_SID        S-1-5-4
 *    SERVICE_SID        S-1-5-6
 *    ANONYMOUS_LOGON_SID    S-1-5-7        (aka null logon session)
 *    PROXY_SID        S-1-5-8
 *    SERVER_LOGON_SID    S-1-5-9        (aka domain controller account)
 *    SELF_SID        S-1-5-10    (self RID)
 *    AUTHENTICATED_USER_SID    S-1-5-11
 *    RESTRICTED_CODE_SID    S-1-5-12    (running restricted code)
 *    TERMINAL_SERVER_SID    S-1-5-13    (running on terminal server)
 *
 *    (Logon IDs)        S-1-5-5-X-Y
 *
 *    (NT non-unique IDs)    S-1-5-0x15-...
 *
 *    (Built-in domain)    S-1-5-0x20
 */

/**
 * union SID_IDENTIFIER_AUTHORITY - A 48-bit value used in the SID structure
 *
 * NOTE: This is stored as a big endian number.
 */
#if 0
typedef union {
    struct {
        WORD high_part;                     /* High 16-bits. */
        DWORD low_part;                     /* Low 32-bits. */
    };
    BYTE value[6];                          /* Value as individual bytes. */
}SID_IDENTIFIER_AUTHORITY;

#endif
/**
 * struct SID -
 *
 * The SID structure is a variable-length structure used to uniquely identify
 * users or groups. SID stands for security identifier.
 *
 * The standard textual representation of the SID is of the form:
 *    S-R-I-S-S...
 * Where:
 *    - The first "S" is the literal character 'S' identifying the following
 *    digits as a SID.
 *    - R is the revision level of the SID expressed as a sequence of digits
 *    in decimal.
 *    - I is the 48-bit identifier_authority, expressed as digits in decimal,
 *    if I < 2^32, or hexadecimal prefixed by "0x", if I >= 2^32.
 *    - S... is one or more sub_authority values, expressed as digits in
 *    decimal.
 *
 * Example SID; the domain-relative SID of the local Administrators group on
 * Windows NT/2k:
 *    S-1-5-32-544
 * This translates to a SID with:
 *    revision = 1,
 *    sub_authority_count = 2,
 *    identifier_authority = {0,0,0,0,0,5}, // SECURITY_NT_AUTHORITY
 *    sub_authority[0] = 32,                // SECURITY_BUILTIN_DOMAIN_RID
 *    sub_authority[1] = 544                // DOMAIN_ALIAS_RID_ADMINS
 */
#if 0
typedef struct {
    BYTE revision;
    BYTE sub_authority_count;
    SID_IDENTIFIER_AUTHORITY identifier_authority;
    DWORD sub_authority[1];                 /* At least one sub_authority. */
}SID,*PSID;
#endif
/**
 * enum SID_CONSTANTS - Current constants for SIDs.
 */
#if 0
typedef enum {
    SID_REVISION                    =  1,   /* Current revision level. */
    SID_MAX_SUB_AUTHORITIES         = 15,   /* Maximum number of those. */
    SID_RECOMMENDED_SUB_AUTHORITIES =  1,   /* Will change to around 6 in
                                            a future revision. */
} SID_CONSTANTS;
#endif

/**
 * enum ACE_TYPES - The predefined ACE types (8-bit, see below).
 */
#if 0
typedef enum {
    ACCESS_MIN_MS_ACE_TYPE          = 0,
    ACCESS_ALLOWED_ACE_TYPE         = 0,
    ACCESS_DENIED_ACE_TYPE          = 1,
    SYSTEM_AUDIT_ACE_TYPE           = 2,
    SYSTEM_ALARM_ACE_TYPE           = 3,    /* Not implemented as of Win2k. */
    ACCESS_MAX_MS_V2_ACE_TYPE       = 3,

    ACCESS_ALLOWED_COMPOUND_ACE_TYPE= 4,
    ACCESS_MAX_MS_V3_ACE_TYPE       = 4,

    /* The following are Win2k only*/
    ACCESS_MIN_MS_OBJECT_ACE_TYPE   = 5,
    ACCESS_ALLOWED_OBJECT_ACE_TYPE  = 5,
    ACCESS_DENIED_OBJECT_ACE_TYPE   = 6,
    SYSTEM_AUDIT_OBJECT_ACE_TYPE    = 7,
    SYSTEM_ALARM_OBJECT_ACE_TYPE    = 8,
    ACCESS_MAX_MS_OBJECT_ACE_TYPE   = 8,

    ACCESS_MAX_MS_V4_ACE_TYPE       = 8,

    /* This one is for WinNT&2k. */
    ACCESS_MAX_MS_ACE_TYPE          = 8,
}ACE_TYPES;
#endif
/**
 * enum ACE_FLAGS - The ACE flags (8-bit) for audit and inheritance.
 *
 * SUCCESSFUL_ACCESS_ACE_FLAG is only used with system audit and alarm ACE
 * types to indicate that a message is generated (in Windows!) for successful
 * accesses.
 *
 * FAILED_ACCESS_ACE_FLAG is only used with system audit and alarm ACE types
 * to indicate that a message is generated (in Windows!) for failed accesses.
 */
#if 0
typedef enum {
    /* The inheritance flags. */
    OBJECT_INHERIT_ACE              = 0x01,
    CONTAINER_INHERIT_ACE           = 0x02,
    NO_PROPAGATE_INHERIT_ACE        = 0x04,
    INHERIT_ONLY_ACE                = 0x08,
    INHERITED_ACE                   = 0x10, /* Win2k only. */
    VALID_INHERIT_FLAGS             = 0x1f,

    /* The audit flags. */
    SUCCESSFUL_ACCESS_ACE_FLAG      = 0x40,
    FAILED_ACCESS_ACE_FLAG          = 0x80,
}ACE_FLAGS;
#endif

/**
 * struct ACE_HEADER -
 *
 * An ACE is an access-control entry in an access-control list (ACL).
 * An ACE defines access to an object for a specific user or group or defines
 * the types of access that generate system-administration messages or alarms
 * for a specific user or group. The user or group is identified by a security
 * identifier (SID).
 *
 * Each ACE starts with an ACE_HEADER structure (aligned on 4-byte boundary),
 * which specifies the type and size of the ACE. The format of the subsequent
 * data depends on the ACE type.
 */
#if 0
typedef struct {
    BYTE type;                              /* Type of the ACE. ACE_TYPES */
    BYTE flags;                             /* Flags describing the ACE.,ACE_FLAGS */
    WORD size;                              /* Size in bytes of the ACE. */
} ACE_HEADER;
#endif
/**
 * enum ACCESS_MASK - The access mask (32-bit).
 *
 * Defines the access rights.
 */
#if 0
typedef enum {
    /*
     * The specific rights (bits 0 to 15). Depend on the type of the
     * object being secured by the ACE.
     */

    /* Specific rights for files and directories are as follows: */

    /* Right to read data from the file. (FILE) */
    FILE_READ_DATA              = 0x00000001,
    /* Right to list contents of a directory. (DIRECTORY) */
    FILE_LIST_DIRECTORY         = 0x00000001,

    /* Right to write data to the file. (FILE) */
    FILE_WRITE_DATA             = 0x00000002,
    /* Right to create a file in the directory. (DIRECTORY) */
    FILE_ADD_FILE               = 0x00000002,

    /* Right to append data to the file. (FILE) */
    FILE_APPEND_DATA            = 0x00000004,
    /* Right to create a subdirectory. (DIRECTORY) */
    FILE_ADD_SUBDIRECTORY       = 0x00000004,

    /* Right to read extended attributes. (FILE/DIRECTORY) */
    FILE_READ_EA                = 0x00000008,

    /* Right to write extended attributes. (FILE/DIRECTORY) */
    FILE_WRITE_EA               = 0x00000010,

    /* Right to execute a file. (FILE) */
    FILE_EXECUTE                = 0x00000020,
    /* Right to traverse the directory. (DIRECTORY) */
    FILE_TRAVERSE               = 0x00000020,

    /*
     * Right to delete a directory and all the files it contains (its
     * children), even if the files are read-only. (DIRECTORY)
     */
    FILE_DELETE_CHILD           = 0x00000040,

    /* Right to read file attributes. (FILE/DIRECTORY) */
    FILE_READ_ATTRIBUTES        = 0x00000080,

    /* Right to change file attributes. (FILE/DIRECTORY) */
    FILE_WRITE_ATTRIBUTES       = 0x00000100,

    /*
     * The standard rights (bits 16 to 23). Are independent of the type of
     * object being secured.
     */

    /* Right to delete the object. */
    DELETE                      = 0x00010000,

    /*
     * Right to read the information in the object's security descriptor,
     * not including the information in the SACL. I.e. right to read the
     * security descriptor and owner.
     */
    READ_CONTROL                = 0x00020000,

    /* Right to modify the DACL in the object's security descriptor. */
    WRITE_DAC                   = 0x00040000,

    /* Right to change the owner in the object's security descriptor. */
    WRITE_OWNER                 = 0x00080000,

    /*
     * Right to use the object for synchronization. Enables a process to
     * wait until the object is in the signalled state. Some object types
     * do not support this access right.
     */
    SYNCHRONIZE                 = 0x00100000,

    /*
     * The following STANDARD_RIGHTS_* are combinations of the above for
     * convenience and are defined by the Win32 API.
     */

    /* These are currently defined to READ_CONTROL. */
    STANDARD_RIGHTS_READ        = 0x00020000,
    STANDARD_RIGHTS_WRITE       = 0x00020000,
    STANDARD_RIGHTS_EXECUTE     = 0x00020000,

    /* Combines DELETE, READ_CONTROL, WRITE_DAC, and WRITE_OWNER access. */
    STANDARD_RIGHTS_REQUIRED    = const_cpu_to_le32(0x000f0000),

    /*
     * Combines DELETE, READ_CONTROL, WRITE_DAC, WRITE_OWNER, and
     * SYNCHRONIZE access.
     */
    STANDARD_RIGHTS_ALL         = 0x001f0000,

    /*
     * The access system ACL and maximum allowed access types (bits 24 to
     * 25, bits 26 to 27 are reserved).
     */
    ACCESS_SYSTEM_SECURITY      = 0x01000000,
    MAXIMUM_ALLOWED             = 0x02000000,

    /*
     * The generic rights (bits 28 to 31). These map onto the standard and
     * specific rights.
     */

    /* Read, write, and execute access. */
    GENERIC_ALL                 = 0x10000000,

    /* Execute access. */
    GENERIC_EXECUTE             = 0x20000000,

    /*
     * Write access. For files, this maps onto:
     *    FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_DATA |
     *    FILE_WRITE_EA | STANDARD_RIGHTS_WRITE | SYNCHRONIZE
     * For directories, the mapping has the same numerical value. See
     * above for the descriptions of the rights granted.
     */
    GENERIC_WRITE               = 0x40000000,

    /*
     * Read access. For files, this maps onto:
     *    FILE_READ_ATTRIBUTES | FILE_READ_DATA | FILE_READ_EA |
     *    STANDARD_RIGHTS_READ | SYNCHRONIZE
     * For directories, the mapping has the same numerical value. See
     * above for the descriptions of the rights granted.
     */
    GENERIC_READ                = 0x80000000,
} ACCESS_MASK;
#endif
/**
 * struct GENERIC_MAPPING -
 *
 * The generic mapping array. Used to denote the mapping of each generic
 * access right to a specific access mask.
 *
 * FIXME: What exactly is this and what is it for? (AIA)
 */
#if 0
typedef struct {
    ACCESS_MASK generic_read;
    ACCESS_MASK generic_write;
    ACCESS_MASK generic_execute;
    ACCESS_MASK generic_all;
} GENERIC_MAPPING;
#endif
/*
 * The predefined ACE type structures are as defined below.
 */

/**
 * struct ACCESS_DENIED_ACE -
 *
 * ACCESS_ALLOWED_ACE, ACCESS_DENIED_ACE, SYSTEM_AUDIT_ACE, SYSTEM_ALARM_ACE
 */
#if 0
typedef struct {
/*0x00*/ACE_HEADER header;
/*0x04*/ACCESS_MASK mask;                   /* Access mask associated with the ACE. */
/*0x08*/SID sid;                            /* The SID associated with the ACE. */
} ACCESS_ALLOWED_ACE, ACCESS_DENIED_ACE,SYSTEM_AUDIT_ACE, SYSTEM_ALARM_ACE;
#endif
/**
 * enum OBJECT_ACE_FLAGS - The object ACE flags (32-bit).
 */
#if 0
typedef enum {
    ACE_OBJECT_TYPE_PRESENT             = 1,
    ACE_INHERITED_OBJECT_TYPE_PRESENT   = 2,
} OBJECT_ACE_FLAGS;
#endif
/**
 * struct ACCESS_ALLOWED_OBJECT_ACE -
 */
#if 0
typedef struct {
/*0x00*/ACE_HEADER header;
/*0x04*/ACCESS_MASK mask;                   /* Access mask associated with the ACE. */
/*0x08*/OBJECT_ACE_FLAGS object_flags;      /* Flags describing the object ACE. */
/*0x0c*/GUID object_type;
/*0x1c*/GUID inherited_object_type;
/*0x2c*/SID sid;                            /* The SID associated with the ACE. */
} ACCESS_ALLOWED_OBJECT_ACE,ACCESS_DENIED_OBJECT_ACE,
  SYSTEM_AUDIT_OBJECT_ACE,SYSTEM_ALARM_OBJECT_ACE;
#endif
/**
 * struct ACL - An ACL is an access-control list (ACL).
 *
 * An ACL starts with an ACL header structure, which specifies the size of
 * the ACL and the number of ACEs it contains. The ACL header is followed by
 * zero or more access control entries (ACEs). The ACL as well as each ACE
 * are aligned on 4-byte boundaries.
 */
#if 0
typedef struct {
    BYTE revision;                          /* Revision of this ACL. */
    BYTE alignment1;
    WORD size;                              /* Allocated space in bytes for ACL. Includes this
                                            header, the ACEs and the remaining free space. */
    WORD ace_count;                         /* Number of ACEs in the ACL. */
    WORD alignment2;
/* sizeof() = 8 bytes */
} ACL,*PACL;
#endif
/**
 * enum ACL_CONSTANTS - Current constants for ACLs.
 */
#if 0
typedef enum {
    /* Current revision. */
    ACL_REVISION            = 2,
    ACL_REVISION_DS         = 4,

    /* History of revisions. */
    ACL_REVISION1           = 1,
    MIN_ACL_REVISION        = 2,
    ACL_REVISION2           = 2,
    ACL_REVISION3           = 3,
    ACL_REVISION4           = 4,
    MAX_ACL_REVISION        = 4,
} ACL_CONSTANTS;
#endif
/**
 * enum SECURITY_DESCRIPTOR_CONTROL -
 *
 * The security descriptor control flags (16-bit).
 *
 * SE_OWNER_DEFAULTED - This boolean flag, when set, indicates that the
 *    SID pointed to by the Owner field was provided by a
 *    defaulting mechanism rather than explicitly provided by the
 *    original provider of the security descriptor.  This may
 *    affect the treatment of the SID with respect to inheritance
 *    of an owner.
 *
 * SE_GROUP_DEFAULTED - This boolean flag, when set, indicates that the
 *    SID in the Group field was provided by a defaulting mechanism
 *    rather than explicitly provided by the original provider of
 *    the security descriptor.  This may affect the treatment of
 *    the SID with respect to inheritance of a primary group.
 *
 * SE_DACL_PRESENT - This boolean flag, when set, indicates that the
 *    security descriptor contains a discretionary ACL.  If this
 *    flag is set and the Dacl field of the SECURITY_DESCRIPTOR is
 *    null, then a null ACL is explicitly being specified.
 *
 * SE_DACL_DEFAULTED - This boolean flag, when set, indicates that the
 *    ACL pointed to by the Dacl field was provided by a defaulting
 *    mechanism rather than explicitly provided by the original
 *    provider of the security descriptor.  This may affect the
 *    treatment of the ACL with respect to inheritance of an ACL.
 *    This flag is ignored if the DaclPresent flag is not set.
 *
 * SE_SACL_PRESENT - This boolean flag, when set,  indicates that the
 *    security descriptor contains a system ACL pointed to by the
 *    Sacl field.  If this flag is set and the Sacl field of the
 *    SECURITY_DESCRIPTOR is null, then an empty (but present)
 *    ACL is being specified.
 *
 * SE_SACL_DEFAULTED - This boolean flag, when set, indicates that the
 *    ACL pointed to by the Sacl field was provided by a defaulting
 *    mechanism rather than explicitly provided by the original
 *    provider of the security descriptor.  This may affect the
 *    treatment of the ACL with respect to inheritance of an ACL.
 *    This flag is ignored if the SaclPresent flag is not set.
 *
 * SE_SELF_RELATIVE - This boolean flag, when set, indicates that the
 *    security descriptor is in self-relative form.  In this form,
 *    all fields of the security descriptor are contiguous in memory
 *    and all pointer fields are expressed as offsets from the
 *    beginning of the security descriptor.
 */
#if 0
typedef enum {
    SE_OWNER_DEFAULTED          = 0x0001,
    SE_GROUP_DEFAULTED          = 0x0002,
    SE_DACL_PRESENT             = 0x0004,
    SE_DACL_DEFAULTED           = 0x0008,
    SE_SACL_PRESENT             = 0x0010,
    SE_SACL_DEFAULTED           = 0x0020,
    SE_DACL_AUTO_INHERIT_REQ    = 0x0100,
    SE_SACL_AUTO_INHERIT_REQ    = 0x0200,
    SE_DACL_AUTO_INHERITED      = 0x0400,
    SE_SACL_AUTO_INHERITED      = 0x0800,
    SE_DACL_PROTECTED           = 0x1000,
    SE_SACL_PROTECTED           = 0x2000,
    SE_RM_CONTROL_VALID         = 0x4000,
    SE_SELF_RELATIVE            = 0x8000,
} SECURITY_DESCRIPTOR_CONTROL;
#endif
/**
 * struct SECURITY_DESCRIPTOR_RELATIVE -
 *
 * Self-relative security descriptor. Contains the owner and group SIDs as well
 * as the sacl and dacl ACLs inside the security descriptor itself.
 */
#if 0
typedef struct {
/*0x00*/BYTE revision;                      /* Revision level of the security descriptor. */
/*0x01*/BYTE alignment;
/*0x02*/WORD control;                       /* Flags qualifying the type of
                                            the descriptor as well as the following fields.SECURITY_DESCRIPTOR_CONTROL */
/*0x04*/DWORD owner;                        /* Byte offset to a SID representing an object's
                                            owner. If this is NULL, no owner SID is present in
                                            the descriptor. */
/*0x08*/DWORD group;                        /* Byte offset to a SID representing an object's
                                            primary group. If this is NULL, no primary group
                                            SID is present in the descriptor. */
/*0x0c*/DWORD sacl;                         /* Byte offset to a system ACL. Only valid, if
                                            SE_SACL_PRESENT is set in the control field. If
                                            SE_SACL_PRESENT is set but sacl is NULL, a NULL ACL
                                            is specified. */
/*0x10*/DWORD dacl;                         /* Byte offset to a discretionary ACL. Only valid, if
                                            SE_DACL_PRESENT is set in the control field. If
                                            SE_DACL_PRESENT is set but dacl is NULL, a NULL ACL
                                            (unconditionally granting access) is specified. */
/* sizeof() = 0x14 bytes */
} SECURITY_DESCRIPTOR_RELATIVE;
#endif
/**
 * struct SECURITY_DESCRIPTOR - Absolute security descriptor.
 *
 * Does not contain the owner and group SIDs, nor the sacl and dacl ACLs inside
 * the security descriptor. Instead, it contains pointers to these structures
 * in memory. Obviously, absolute security descriptors are only useful for in
 * memory representations of security descriptors.
 *
 * On disk, a self-relative security descriptor is used.
 */
#if 0
typedef struct {
    BYTE revision;                          /* Revision level of the security descriptor. */
    BYTE alignment;
    WORD control;                           /* Flags qualifying the type of
                                            the descriptor as well as the following fields. SECURITY_DESCRIPTOR_CONTROL*/
    SID *owner;                             /* Points to a SID representing an object's owner. If
                                            this is NULL, no owner SID is present in the
                                            descriptor. */
    SID *group;                             /* Points to a SID representing an object's primary
                                            group. If this is NULL, no primary group SID is
                                            present in the descriptor. */
    ACL *sacl;                              /* Points to a system ACL. Only valid, if
                                            SE_SACL_PRESENT is set in the control field. If
                                            SE_SACL_PRESENT is set but sacl is NULL, a NULL ACL
                                            is specified. */
    ACL *dacl;                              /* Points to a discretionary ACL. Only valid, if
                                            SE_DACL_PRESENT is set in the control field. If
                                            SE_DACL_PRESENT is set but dacl is NULL, a NULL ACL
                                            (unconditionally granting access) is specified. */
}SECURITY_DESCRIPTOR;
#endif
/**
 * enum SECURITY_DESCRIPTOR_CONSTANTS -
 *
 * Current constants for security descriptors.
 */
#if 0
typedef enum {
    /* Current revision. */
    SECURITY_DESCRIPTOR_REVISION        = 1,
    SECURITY_DESCRIPTOR_REVISION1       = 1,

    /* The sizes of both the absolute and relative security descriptors is
       the same as pointers, at least on ia32 architecture are 32-bit. */
    SECURITY_DESCRIPTOR_MIN_LENGTH      = sizeof(SECURITY_DESCRIPTOR),
} SECURITY_DESCRIPTOR_CONSTANTS;
#endif
/*
 * Attribute: Security descriptor (0x50).
 *
 * A standard self-relative security descriptor.
 *
 * NOTE: Can be resident or non-resident.
 * NOTE: Not used in NTFS 3.0+, as security descriptors are stored centrally
 * in FILE_Secure and the correct descriptor is found using the security_id
 * from the standard information attribute.
 */
//typedef SECURITY_DESCRIPTOR_RELATIVE SECURITY_DESCRIPTOR_ATTR;

/*
 * On NTFS 3.0+, all security descriptors are stored in FILE_Secure. Only one
 * referenced instance of each unique security descriptor is stored.
 *
 * FILE_Secure contains no unnamed data attribute, i.e. it has zero length. It
 * does, however, contain two indexes ($SDH and $SII) as well as a named data
 * stream ($SDS).
 *
 * Every unique security descriptor is assigned a unique security identifier
 * (security_id, not to be confused with a SID). The security_id is unique for
 * the NTFS volume and is used as an index into the $SII index, which maps
 * security_ids to the security descriptor's storage location within the $SDS
 * data attribute. The $SII index is sorted by ascending security_id.
 *
 * A simple hash is computed from each security descriptor. This hash is used
 * as an index into the $SDH index, which maps security descriptor hashes to
 * the security descriptor's storage location within the $SDS data attribute.
 * The $SDH index is sorted by security descriptor hash and is stored in a B+
 * tree. When searching $SDH (with the intent of determining whether or not a
 * new security descriptor is already present in the $SDS data stream), if a
 * matching hash is found, but the security descriptors do not match, the
 * search in the $SDH index is continued, searching for a next matching hash.
 *
 * When a precise match is found, the security_id corresponding to the security
 * descriptor in the $SDS attribute is read from the found $SDH index entry and
 * is stored in the $STANDARD_INFORMATION attribute of the file/directory to
 * which the security descriptor is being applied. The $STANDARD_INFORMATION
 * attribute is present in all base mft records (i.e. in all files and
 * directories).
 *
 * If a match is not found, the security descriptor is assigned a new unique
 * security_id and is added to the $SDS data attribute. Then, entries
 * referencing the this security descriptor in the $SDS data attribute are
 * added to the $SDH and $SII indexes.
 *
 * Note: Entries are never deleted from FILE_Secure, even if nothing
 * references an entry any more.
 */

/**
 * struct SECURITY_DESCRIPTOR_HEADER -
 *
 * This header precedes each security descriptor in the $SDS data stream.
 * This is also the index entry data part of both the $SII and $SDH indexes.
 */
typedef struct {
    DWORD hash;                             /* Hash of the security descriptor. */
    DWORD security_id;                      /* The security_id assigned to the descriptor. */
    DWORDLONG offset;                       /* Byte offset of this entry in the $SDS stream. */
    DWORD length;                           /* Size in bytes of this entry in $SDS stream. */
} SECURITY_DESCRIPTOR_HEADER;

/**
 * struct SDH_INDEX_DATA -
 */
typedef struct {
    DWORD hash;                             /* Hash of the security descriptor. */
    DWORD security_id;                      /* The security_id assigned to the descriptor. */
    DWORDLONG offset;                       /* Byte offset of this entry in the $SDS stream. */
    DWORD length;                           /* Size in bytes of this entry in $SDS stream. */
    DWORD reserved_II;                      /* Padding - always unicode "II" or zero. This field
                                            isn't counted in INDEX_ENTRY's data_length. */
} SDH_INDEX_DATA;

/**
 * struct SII_INDEX_DATA -
 */
typedef SECURITY_DESCRIPTOR_HEADER SII_INDEX_DATA;

/**
 * struct SDS_ENTRY -
 *
 * The $SDS data stream contains the security descriptors, aligned on 16-byte
 * boundaries, sorted by security_id in a B+ tree. Security descriptors cannot
 * cross 256kib boundaries (this restriction is imposed by the Windows cache
 * manager). Each security descriptor is contained in a SDS_ENTRY structure.
 * Also, each security descriptor is stored twice in the $SDS stream with a
 * fixed offset of 0x40000 bytes (256kib, the Windows cache manager's max size)
 * between them; i.e. if a SDS_ENTRY specifies an offset of 0x51d0, then the
 * the first copy of the security descriptor will be at offset 0x51d0 in the
 * $SDS data stream and the second copy will be at offset 0x451d0.
 */
typedef struct {
/*0x00*/SECURITY_DESCRIPTOR_HEADER header;
/*0x14*/SECURITY_DESCRIPTOR_RELATIVE sid;   /* The self-relative security
                                            descriptor. */
} SDS_ENTRY;

/**
 * struct SII_INDEX_KEY - The index entry key used in the $SII index.
 *
 * The collation type is COLLATION_NTOFS_ULONG.
 */
typedef struct {
    DWORD security_id;   /* The security_id assigned to the descriptor. */
} SII_INDEX_KEY;

/**
 * struct SDH_INDEX_KEY - The index entry key used in the $SDH index.
 *
 * The keys are sorted first by hash and then by security_id.
 * The collation rule is COLLATION_NTOFS_SECURITY_HASH.
 */
typedef struct {
    DWORD hash;       /* Hash of the security descriptor. */
    DWORD security_id;   /* The security_id assigned to the descriptor. */
} SDH_INDEX_KEY;

/**
 * struct VOLUME_NAME - Attribute: Volume name (0x60).
 *
 * NOTE: Always resident.
 * NOTE: Present only in FILE_Volume.
 */
typedef struct {
    WCHAR name[0];                          /* The name of the volume in Unicode. */
} VOLUME_NAME;

/**
 * enum VOLUME_FLAGS - Possible flags for the volume (16-bit).
 */
typedef enum {
    VOLUME_IS_DIRTY                 = 0x0001,
    VOLUME_RESIZE_LOG_FILE          = 0x0002,
    VOLUME_UPGRADE_ON_MOUNT         = 0x0004,
    VOLUME_MOUNTED_ON_NT4           = 0x0008,
    VOLUME_DELETE_USN_UNDERWAY      = 0x0010,
    VOLUME_REPAIR_OBJECT_ID         = 0x0020,
    VOLUME_CHKDSK_UNDERWAY          = 0x4000,
    VOLUME_MODIFIED_BY_CHKDSK       = 0x8000,
    VOLUME_FLAGS_MASK               = 0xc03f,
}VOLUME_FLAGS;

/**
 * struct VOLUME_INFORMATION - Attribute: Volume information (0x70).
 *
 * NOTE: Always resident.
 * NOTE: Present only in FILE_Volume.
 * NOTE: Windows 2000 uses NTFS 3.0 while Windows NT4 service pack 6a uses
 *     NTFS 1.2. I haven't personally seen other values yet.
 */
typedef struct {
    DWORDLONG reserved;                     /* Not used (yet?). */
    BYTE major_ver;                         /* Major version of the ntfs format. */
    BYTE minor_ver;                         /* Minor version of the ntfs format. */
    WORD flags;                             /* Bit array of VOLUME_* flags.VOLUME_FLAGS */
} VOLUME_INFORMATION;

/**
 * struct DATA_ATTR - Attribute: Data attribute (0x80).
 *
 * NOTE: Can be resident or non-resident.
 *
 * Data contents of a file (i.e. the unnamed stream) or of a named stream.
 */
typedef struct {
    BYTE data[0];                           /* The file's data contents. */
} DATA_ATTR;

/**
 * enum INDEX_HEADER_FLAGS - Index header flags (8-bit).
 */
typedef enum {
    /* When index header is in an index root attribute: */
    SMALL_INDEX    = 0,                     /* The index is small enough to fit inside the
                                            index root attribute and there is no index
                                            allocation attribute present. */
    LARGE_INDEX    = 1,                     /* The index is too large to fit in the index
                                            root attribute and/or an index allocation
                                            attribute is present. */
    /*
     * When index header is in an index block, i.e. is part of index
     * allocation attribute:
     */
    LEAF_NODE    = 0,                       /* This is a leaf node, i.e. there are no more
                                            nodes branching off it. */
    INDEX_NODE    = 1,                      /* This node indexes other nodes, i.e. is not a
                                            leaf node. */
    NODE_MASK    = 1,                       /* Mask for accessing the *_NODE bits. */
} INDEX_HEADER_FLAGS;

/**
 * struct INDEX_HEADER -
 *
 * This is the header for indexes, describing the INDEX_ENTRY records, which
 * follow the INDEX_HEADER. Together the index header and the index entries
 * make up a complete index.
 *
 * IMPORTANT NOTE: The offset, length and size structure members are counted
 * relative to the start of the index header structure and not relative to the
 * start of the index root or index allocation structures themselves.
 */
typedef struct {
/*0x00*/    DWORD entries_offset;           /* Byte offset from the INDEX_HEADER to first
                                            INDEX_ENTRY, aligned to 8-byte boundary.  */
/*0x04*/    DWORD index_length;             /* Data size in byte of the INDEX_ENTRY's,
                                            including the INDEX_HEADER, aligned to 8. */
/*0x08*/    DWORD allocated_size;           /* Allocated byte size of this index (block),
                                            multiple of 8 bytes. See more below.      */
    /* 
       For the index root attribute, the above two numbers are always
       equal, as the attribute is resident and it is resized as needed.
       
       For the index allocation attribute, the attribute is not resident 
       and the allocated_size is equal to the index_block_size specified 
       by the corresponding INDEX_ROOT attribute minus the INDEX_BLOCK 
       size not counting the INDEX_HEADER part (i.e. minus -24).
     */
/*0x0c*/    BYTE ih_flags;                  /* Bit field of INDEX_HEADER_FLAGS,INDEX_HEADER_FLAGS*/
/*0x0d*/    BYTE reserved[3];               /* Reserved/align to 8-byte boundary.*/
/* sizeof() == 16 */
}INDEX_HEADER;

/**
 * struct INDEX_ROOT - Attribute: Index root (0x90).
 *
 * NOTE: Always resident.
 *
 * This is followed by a sequence of index entries (INDEX_ENTRY structures)
 * as described by the index header.
 *
 * When a directory is small enough to fit inside the index root then this
 * is the only attribute describing the directory. When the directory is too
 * large to fit in the index root, on the other hand, two additional attributes
 * are present: an index allocation attribute, containing sub-nodes of the B+
 * directory tree (see below), and a bitmap attribute, describing which virtual
 * cluster numbers (vcns) in the index allocation attribute are in use by an
 * index block.
 *
 * NOTE: The root directory (FILE_root) contains an entry for itself. Other
 * directories do not contain entries for themselves, though.
 */
typedef struct {
/*0x00*/ATTR_TYPES type;                    /* Type of the indexed attribute. Is
                                            $FILE_NAME for directories, zero
                                            for view indexes. No other values
                                            allowed. */
/*0x04*/COLLATION_RULES collation_rule;     /* Collation rule used to sort the
                                            index entries. If type is $FILE_NAME,
                                            this must be COLLATION_FILE_NAME. */
/*0x08*/DWORD index_block_size;             /* Size of index block in bytes (in
                                            the index allocation attribute). */
/*0x0c*/BYTE clusters_per_index_block;      /* Size of index block in clusters (in
                                            the index allocation attribute), when
                                            an index block is >= than a cluster,
                                            otherwise sectors per index block. */
/*0x0d*/BYTE reserved[3];                   /* Reserved/align to 8-byte boundary. */
/*0x10*/INDEX_HEADER index;                 /* Index header describing the
                                            following index entries. */
/* sizeof()= 32 bytes */
}INDEX_ROOT;

/**
 * struct INDEX_BLOCK - Attribute: Index allocation (0xa0).
 *
 * NOTE: Always non-resident (doesn't make sense to be resident anyway!).
 *
 * This is an array of index blocks. Each index block starts with an
 * INDEX_BLOCK structure containing an index header, followed by a sequence of
 * index entries (INDEX_ENTRY structures), as described by the INDEX_HEADER.
 */
typedef struct {
    /*  NTFS_RECORD */
/*0x00*/NTFS_RECORD_TYPES magic;            /* Magic is "INDX". */
/*0x04*/WORD usa_ofs;                       /* See NTFS_RECORD definition. */
/*0x06*/WORD usa_count;                     /* See NTFS_RECORD definition. */

/*0x08*/DWORDLONG lsn;                     /* $LogFile sequence number of the last
                                            modification of this index block. */
/*0x10*/DWORDLONG index_block_vcn;          /* Virtual cluster number of the index block. */
/*0x18*/INDEX_HEADER index;                 /* Describes the following index entries. */
/* sizeof()= 40 (0x28) bytes */
/*
 * When creating the index block, we place the update sequence array at this
 * offset, i.e. before we start with the index entries. This also makes sense,
 * otherwise we could run into problems with the update sequence array
 * containing in itself the last two bytes of a sector which would mean that
 * multi sector transfer protection wouldn't work. As you can't protect data
 * by overwriting it since you then can't get it back...
 * When reading use the data from the ntfs record header.
 */
}INDEX_BLOCK;

typedef INDEX_BLOCK INDEX_ALLOCATION;

/**
 * struct REPARSE_INDEX_KEY -
 *
 * The system file FILE_Extend/$Reparse contains an index named $R listing
 * all reparse points on the volume. The index entry keys are as defined
 * below. Note, that there is no index data associated with the index entries.
 *
 * The index entries are sorted by the index key file_id. The collation rule is
 * COLLATION_NTOFS_ULONGS. FIXME: Verify whether the reparse_tag is not the
 * primary key / is not a key at all. (AIA)
 */
typedef struct {
    DWORD reparse_tag;                      /* Reparse point type (inc. flags). */
    MFT_REF file_id;                        /* Mft record of the file containing the
                                            reparse point attribute. */
}REPARSE_INDEX_KEY;

/**
 * enum QUOTA_FLAGS - Quota flags (32-bit).
 */
typedef enum {
    /* The user quota flags. Names explain meaning. */
    QUOTA_FLAG_DEFAULT_LIMITS       = 0x00000001,
    QUOTA_FLAG_LIMIT_REACHED        = 0x00000002,
    QUOTA_FLAG_ID_DELETED           = 0x00000004,

    QUOTA_FLAG_USER_MASK            = 0x00000007,
        /* Bit mask for user quota flags. */

    /* These flags are only present in the quota defaults index entry,
       i.e. in the entry where owner_id = QUOTA_DEFAULTS_ID. */
    QUOTA_FLAG_TRACKING_ENABLED     = 0x00000010,
    QUOTA_FLAG_ENFORCEMENT_ENABLED  = 0x00000020,
    QUOTA_FLAG_TRACKING_REQUESTED   = 0x00000040,
    QUOTA_FLAG_LOG_THRESHOLD        = 0x00000080,
    QUOTA_FLAG_LOG_LIMIT            = 0x00000100,
    QUOTA_FLAG_OUT_OF_DATE          = 0x00000200,
    QUOTA_FLAG_CORRUPT              = 0x00000400,
    QUOTA_FLAG_PENDING_DELETES      = 0x00000800,
} QUOTA_FLAGS;

/**
 * struct QUOTA_CONTROL_ENTRY -
 *
 * The system file FILE_Extend/$Quota contains two indexes $O and $Q. Quotas
 * are on a per volume and per user basis.
 *
 * The $Q index contains one entry for each existing user_id on the volume. The
 * index key is the user_id of the user/group owning this quota control entry,
 * i.e. the key is the owner_id. The user_id of the owner of a file, i.e. the
 * owner_id, is found in the standard information attribute. The collation rule
 * for $Q is COLLATION_NTOFS_ULONG.
 *
 * The $O index contains one entry for each user/group who has been assigned
 * a quota on that volume. The index key holds the SID of the user_id the
 * entry belongs to, i.e. the owner_id. The collation rule for $O is
 * COLLATION_NTOFS_SID.
 *
 * The $O index entry data is the user_id of the user corresponding to the SID.
 * This user_id is used as an index into $Q to find the quota control entry
 * associated with the SID.
 *
 * The $Q index entry data is the quota control entry and is defined below.
 */
typedef struct {
    DWORD version;                          /* Currently equals 2. */
    QUOTA_FLAGS flags;                      /* Flags describing this quota entry. */
    DWORDLONG bytes_used;                   /* How many bytes of the quota are in use. */
    DWORDLONG change_time;                  /* Last time this quota entry was changed. */
    DWORDLONG threshold;                    /* Soft quota (-1 if not limited). */
    DWORDLONG limit;                        /* Hard quota (-1 if not limited). */
    DWORDLONG exceeded_time;                /* How long the soft quota has been exceeded. */
/* The below field is NOT present for the quota defaults entry. */
    SID sid;                                /* The SID of the user/object associated with
                                            this quota entry. If this field is missing
                                            then the INDEX_ENTRY is padded with zeros
                                            to multiply of 8 which are not counted in
                                            the data_length field. If the sid is present
                                            then this structure is padded with zeros to
                                            multiply of 8 and the padding is counted in
                                            the INDEX_ENTRY's data_length. */
}QUOTA_CONTROL_ENTRY;

/**
 * struct QUOTA_O_INDEX_DATA -
 */
typedef struct {
    DWORD owner_id;
    DWORD unknown;                          /* Always 32. Seems to be padding and it's not
                                            counted in the INDEX_ENTRY's data_length.
                                            This field shouldn't be really here. */
} QUOTA_O_INDEX_DATA;

/**
 * enum PREDEFINED_OWNER_IDS - Predefined owner_id values (32-bit).
 */
typedef enum {
    QUOTA_INVALID_ID            = 0x00000000,
    QUOTA_DEFAULTS_ID           = 0x00000001,
    QUOTA_FIRST_USER_ID         = 0x00000100,
} PREDEFINED_OWNER_IDS;

/**
 * enum INDEX_ENTRY_FLAGS - Index entry flags (16-bit).
 */
typedef enum {
    INDEX_ENTRY_NODE = (1),                 /* This entry contains a
                                            sub-node, i.e. a reference to an index
                                            block in form of a virtual cluster
                                            number (see below). */
    INDEX_ENTRY_END  = (2),                 /* This signifies the last
                                            entry in an index block. The index
                                            entry does not represent a file but it
                                            can point to a sub-node. */
    INDEX_ENTRY_SPACE_FILLER = 0xffff,      /* Just to force 16-bit width. */
} INDEX_ENTRY_FLAGS;

/**
 * struct INDEX_ENTRY_HEADER - This the index entry header (see below).
 * 
 *         ==========================================================
 *         !!!!!  SEE DESCRIPTION OF THE FIELDS AT INDEX_ENTRY  !!!!!
 *         ==========================================================
 */
typedef struct {
/*0x00*/union {                             /* Only valid when INDEX_ENTRY_END is not set. */
/*0x00*/        MFT_REF indexed_file;       /* The mft reference of the file
						                    described by this index
						                    entry. Used for directory
						                    indexes. */
                struct {                    /* Used for views/indexes to find the entry's data. */
/*0x00*/            WORD data_offset;       /* Data byte offset from this
						                    INDEX_ENTRY. Follows the
						                    index key. */
/*0x02*/            WORD data_length;       /* Data length in bytes. */
/*0x04*/            DWORD reservedV;        /* Reserved (zero). */
                };
        };
/*0x08*/WORD length;                        /* Byte size of this index entry, multiple of
				                            8-bytes. Size includes INDEX_ENTRY_HEADER
				                            and the optional subnode VCN. See below. */
/*0x0a*/WORD key_length;                    /* Byte size of the key value, which is in the
				                            index entry. It follows field reserved. Not
				                            multiple of 8-bytes. */
/*0x0c*/INDEX_ENTRY_FLAGS flags;            /* Bit field of INDEX_ENTRY_* flags. */
/*0x0e*/WORD reserved;                      /* Reserved/align to 8-byte boundary. */
/* sizeof() = 16 bytes */
}INDEX_ENTRY_HEADER;

/**
 * struct INDEX_ENTRY - This is an index entry.
 *
 * A sequence of such entries follows each INDEX_HEADER structure. Together
 * they make up a complete index. The index follows either an index root
 * attribute or an index allocation attribute.
 *
 * NOTE: Before NTFS 3.0 only filename attributes were indexed.
 */
typedef struct {
/*0x00*/INDEX_ENTRY_HEADER  header;
/*0x10*/union {                             /* The key of the indexed attribute. NOTE: Only present
                                            if INDEX_ENTRY_END bit in flags is not set. NOTE: On
                                            NTFS versions before 3.0 the only valid key is the
                                            FILE_NAME_ATTR. On NTFS 3.0+ the following
                                            additional index keys are defined: */
            FILE_NAME_ATTR file_name;       /* $I30 index in directories. */
            SII_INDEX_KEY sii;              /* $SII index in $Secure. */
            SDH_INDEX_KEY sdh;              /* $SDH index in $Secure. */
            GUID object_id;                 /* $O index in FILE_Extend/$ObjId: The
                                            object_id of the mft record found in
                                            the data part of the index. */
            REPARSE_INDEX_KEY reparse;      /* $R index in
                                            FILE_Extend/$Reparse. */
            SID sid;                        /* $O index in FILE_Extend/$Quota:
                                            SID of the owner of the user_id. */
            DWORD owner_id;                 /* $Q index in FILE_Extend/$Quota:
                                            user_id of the owner of the quota
                                            control entry in the data part of
                                            the index. */
    }key;
    /* The (optional) index data is inserted here when creating.
    VCN vcn;       If INDEX_ENTRY_NODE bit in ie_flags is set, the last
               eight bytes of this index entry contain the virtual
               cluster number of the index block that holds the
               entries immediately preceding the current entry. 
    
               If the key_length is zero, then the vcn immediately
               follows the INDEX_ENTRY_HEADER. 
    
               The address of the vcn of "ie" INDEX_ENTRY is given by 
               (char*)ie + le16_to_cpu(ie->length) - sizeof(VCN)
    */
} INDEX_ENTRY,*PINDEX_ENTRY;

/**
 * struct BITMAP_ATTR - Attribute: Bitmap (0xb0).
 *
 * Contains an array of bits (aka a bitfield).
 *
 * When used in conjunction with the index allocation attribute, each bit
 * corresponds to one index block within the index allocation attribute. Thus
 * the number of bits in the bitmap * index block size / cluster size is the
 * number of clusters in the index allocation attribute.
 */
typedef struct {
    BYTE bitmap[0];            /* Array of bits. */
}BITMAP_ATTR;

/**
 * enum PREDEFINED_REPARSE_TAGS -
 *
 * The reparse point tag defines the type of the reparse point. It also
 * includes several flags, which further describe the reparse point.
 *
 * The reparse point tag is an unsigned 32-bit value divided in three parts:
 *
 * 1. The least significant 16 bits (i.e. bits 0 to 15) specify the type of
 *    the reparse point.
 * 2. The 13 bits after this (i.e. bits 16 to 28) are reserved for future use.
 * 3. The most significant three bits are flags describing the reparse point.
 *    They are defined as follows:
 *    bit 29: Name surrogate bit. If set, the filename is an alias for
 *        another object in the system.
 *    bit 30: High-latency bit. If set, accessing the first byte of data will
 *        be slow. (E.g. the data is stored on a tape drive.)
 *    bit 31: Microsoft bit. If set, the tag is owned by Microsoft. User
 *        defined tags have to use zero here.
 */

typedef enum {
    IO_REPARSE_TAG_IS_ALIAS         = 0x20000000,
    IO_REPARSE_TAG_IS_HIGH_LATENCY  = 0x40000000,
    IO_REPARSE_TAG_IS_MICROSOFT     = 0x80000000,

//  IO_REPARSE_TAG_RESERVED_ZERO    = 0x00000000,
//  IO_REPARSE_TAG_RESERVED_ONE     = 0x00000001,
//  IO_REPARSE_TAG_RESERVED_RANGE   = 0x00000001,

    IO_REPARSE_TAG_NSS              = 0x68000005,
    IO_REPARSE_TAG_NSS_RECOVER      = 0x68000006,
//  IO_REPARSE_TAG_SIS              = 0x68000007,
//  IO_REPARSE_TAG_DFS              = 0x68000008,

//  IO_REPARSE_TAG_MOUNT_POINT      = 0x88000003,

//  IO_REPARSE_TAG_HSM              = 0xa8000004,

    IO_REPARSE_TAG_SYMBOLIC_LINK    = 0xe8000000,

    IO_REPARSE_TAG_VALID_VALUES     = 0xe000ffff,
} PREDEFINED_REPARSE_TAGS;

/**
 * struct REPARSE_POINT - Attribute: Reparse point (0xc0).
 *
 * NOTE: Can be resident or non-resident.
 */
typedef struct {
    DWORD reparse_tag;                      /* Reparse point type (inc. flags). */
    WORD reparse_data_length;               /* Byte size of reparse data. */
    WORD reserved;                          /* Align to 8-byte boundary. */
    BYTE reparse_data[0];                   /* Meaning depends on reparse_tag. */
} REPARSE_POINT;

/**
 * struct EA_INFORMATION - Attribute: Extended attribute information (0xd0).
 *
 * NOTE: Always resident.
 */
typedef struct {
    WORD ea_length;                         /* Byte size of the packed extended
                                            attributes. */
    WORD need_ea_count;                     /* The number of extended attributes which have
                                            the NEED_EA bit set. */
    DWORD ea_query_length;                  /* Byte size of the buffer required to query
                                            the extended attributes when calling
                                            ZwQueryEaFile() in Windows NT/2k. I.e. the
                                            byte size of the unpacked extended
                                            attributes. */
} EA_INFORMATION;

/**
 * enum EA_FLAGS - Extended attribute flags (8-bit).
 */
typedef enum {
    NEED_EA    = 0x80,                      /* Indicate that the file to which the EA
                                            belongs cannot be interpreted without
                                            understanding the associated extended
                                            attributes. */
}EA_FLAGS;

/**
 * struct EA_ATTR - Attribute: Extended attribute (EA) (0xe0).
 *
 * Like the attribute list and the index buffer list, the EA attribute value is
 * a sequence of EA_ATTR variable length records.
 *
 * FIXME: It appears weird that the EA name is not Unicode. Is it true?
 * FIXME: It seems that name is always uppercased. Is it true?
 */
typedef struct {
    DWORD next_entry_offset;                /* Offset to the next EA_ATTR. */
    BYTE flags;                             /* Flags describing the EA. EA_FLAGS */
    BYTE name_length;                       /* Length of the name of the extended
                                            attribute in bytes. */
    WORD value_length;                      /* Byte size of the EA's value. */
    BYTE name_and_value[0];                 /* Name of the EA. */                          /* The value of the EA. Immediately
                                            follows the name. */
} EA_ATTR;

/**
 * struct PROPERTY_SET - Attribute: Property set (0xf0).
 *
 * Intended to support Native Structure Storage (NSS) - a feature removed from
 * NTFS 3.0 during beta testing.
 */
typedef struct {
    /* Irrelevant as feature unused. */
} PROPERTY_SET;

/**
 * struct LOGGED_UTILITY_STREAM - Attribute: Logged utility stream (0x100).
 *
 * NOTE: Can be resident or non-resident.
 *
 * Operations on this attribute are logged to the journal ($LogFile) like
 * normal metadata changes.
 *
 * Used by the Encrypting File System (EFS).  All encrypted files have this
 * attribute with the name $EFS.  See below for the relevant structures.
 */
typedef struct {
    /* Can be anything the creator chooses. */
} LOGGED_UTILITY_STREAM;

/*
 * $EFS Data Structure:
 *
 * The following information is about the data structures that are contained
 * inside a logged utility stream (0x100) with a name of "$EFS".
 *
 * The stream starts with an instance of EFS_ATTR_HEADER.
 *
 * Next, at offsets offset_to_ddf_array and offset_to_drf_array (unless any of
 * them is 0) there is a EFS_DF_ARRAY_HEADER immediately followed by a sequence
 * of multiple data decryption/recovery fields.
 *
 * Each data decryption/recovery field starts with a EFS_DF_HEADER and the next
 * one (if it exists) can be found by adding EFS_DF_HEADER->df_length bytes to
 * the offset of the beginning of the current EFS_DF_HEADER.
 *
 * The data decryption/recovery field contains an EFS_DF_CERTIFICATE_HEADER, a
 * SID, an optional GUID, an optional container name, a non-optional user name,
 * and the encrypted FEK.
 *
 * Note all the below are best guesses so may have mistakes/inaccuracies.
 * Corrections/clarifications/additions are always welcome!
 *
 * Ntfs.sys takes an EFS value length of <= 0x54 or > 0x40000 to BSOD, i.e. it
 * is invalid.
 */

/**
 * struct EFS_ATTR_HEADER - "$EFS" header.
 *
 * The header of the Logged utility stream (0x100) attribute named "$EFS".
 */
typedef struct {
/*0x00*/DWORD length;                       /* Length of EFS attribute in bytes. */
/*0x04*/DWORD state;                        /* Always 0? */
/*0x08*/DWORD version;                      /* Efs version.  Always 2? */
/*0x0c*/DWORD crypto_api_version;           /* Always 0? */
/*0x10*/BYTE unknown4[16];                  /* MD5 hash of decrypted FEK?  This field is
                                            created with a call to UuidCreate() so is
                                            unlikely to be an MD5 hash and is more
                                            likely to be GUID of this encrytped file
                                            or something like that. */
/*0x20*/BYTE unknown5[16];                  /* MD5 hash of DDFs? */
/*0x30*/BYTE unknown6[16];                  /* MD5 hash of DRFs? */
/*0x40*/DWORD offset_to_ddf_array;          /* Offset in bytes to the array of data
                                            decryption fields (DDF), see below.  Zero if
                                            no DDFs are present. */
/*0x44*/DWORD offset_to_drf_array;          /* Offset in bytes to the array of data
                                            recovery fields (DRF), see below.  Zero if
                                            no DRFs are present. */
/*0x48*/DWORD reserved;                     /* Reserved. */
} EFS_ATTR_HEADER;

/**
 * struct EFS_DF_ARRAY_HEADER -
 */
typedef struct {
    DWORD df_count;                         /* Number of data decryption/recovery fields in
                                            the array. */
} EFS_DF_ARRAY_HEADER;

/**
 * struct EFS_DF_HEADER -
 */
typedef struct {
/*0x00*/DWORD df_length;                    /* Length of this data decryption/recovery
                                            field in bytes. */
/*0x04*/DWORD cred_header_offset;           /* Offset in bytes to the credential header. */
/*0x08*/DWORD fek_size;                     /* Size in bytes of the encrypted file
                                            encryption key (FEK). */
/*0x0c*/DWORD fek_offset;                   /* Offset in bytes to the FEK from the start of
                                            the data decryption/recovery field. */
/*0x10*/DWORD unknown1;                     /* always 0?  Might be just padding. */
} EFS_DF_HEADER;

/**
 * struct EFS_DF_CREDENTIAL_HEADER -
 */
typedef struct {
/*0x00*/DWORD cred_length;                  /* Length of this credential in bytes. */
/*0x04*/DWORD sid_offset;                   /* Offset in bytes to the user's sid from start
                                            of this structure.  Zero if no sid is
                                            present. */
/*/*0x08*/DWORD type;                       /* Type of this credential:
                                            1 = CryptoAPI container.
                                            2 = Unexpected type.
                                            3 = Certificate thumbprint.
                                            other = Unknown type. */
    union {
        /* CryptoAPI container. */
        struct {
/*0x0c*/    DWORD container_name_offset;    /* Offset in bytes to
                                            the name of the container from start of this
                                            structure (may not be zero). */
/*0x10*/    DWORD provider_name_offset;     /* Offset in bytes to
                                            the name of the provider from start of this
                                            structure (may not be zero). */
/*0x14*/    DWORD public_key_blob_offset;   /* Offset in bytes to
                                            the public key blob from start of this
                                            structure. */
/*0x18*/    DWORD public_key_blob_size;     /* Size in bytes of
                                            public key blob. */
        };
        /* Certificate thumbprint. */
        struct {
/*0x0c*/    DWORD cert_thumbprint_header_size;/* Size in
                                            bytes of the header of the certificate
                                            thumbprint. */
/*0x10*/    DWORD cert_thumbprint_header_offset;/* Offset in
                                            bytes to the header of the certificate
                                            thumbprint from start of this structure. */
/*0x18*/    DWORD unknown1;                 /* Always 0?  Might be padding... */
/*0x18*/    DWORD unknown2;                 /* Always 0?  Might be padding... */
        };
    };
}EFS_DF_CREDENTIAL_HEADER;

typedef EFS_DF_CREDENTIAL_HEADER EFS_DF_CRED_HEADER;

/**
 * struct EFS_DF_CERTIFICATE_THUMBPRINT_HEADER -
 */
typedef struct {
/*0x00*/DWORD thumbprint_offset;            /* Offset in bytes to the thumbprint. */
/*0x04*/DWORD thumbprint_size;              /* Size of thumbprint in bytes. */
/*0x08*/DWORD container_name_offset;        /* Offset in bytes to the name of the
                                            container from start of this
                                            structure or 0 if no name present. */
/*0x0c*/DWORD provider_name_offset;         /* Offset in bytes to the name of the
                                            cryptographic provider from start of
                                            this structure or 0 if no name
                                            present. */
/*0x10*/DWORD user_name_offset;             /* Offset in bytes to the user name
                                            from start of this structure or 0 if
                                            no user name present.  (This is also
                                            known as lpDisplayInformation.) */
} EFS_DF_CERTIFICATE_THUMBPRINT_HEADER;

typedef EFS_DF_CERTIFICATE_THUMBPRINT_HEADER EFS_DF_CERT_THUMBPRINT_HEADER;

#if 0
typedef enum {
    INTX_SYMBOLIC_LINK =
        const_cpu_to_le64(0x014B4E4C78746E49ULL), /* "IntxLNK\1" */
    INTX_CHARACTER_DEVICE =
        const_cpu_to_le64(0x0052484378746E49ULL), /* "IntxCHR\0" */
    INTX_BLOCK_DEVICE =
        const_cpu_to_le64(0x004B4C4278746E49ULL), /* "IntxBLK\0" */
} INTX_FILE_TYPES;
#endif

//INTX_FILE_TYPES
#define INTX_SYMBOLIC_LINK      (0x014B4E4C78746E49ull)
#define INTX_CHARACTER_DEVICE   (0x0052484378746E49ull)
#define INTX_BLOCK_DEVICE       (0x004B4C4278746E49ull)

typedef struct {
    DWORDLONG magic;                        /* Intx file magic. INTX_FILE_TYPES */
    union {
        /* For character and block devices. */
        struct {
            DWORDLONG major;                /* Major device number. */
            DWORDLONG minor;                /* Minor device number. */
            void *device_end[0];            /* Marker for offsetof(). */
        };
        /* For symbolic links. */
        WCHAR target[0];
    };
}INTX_FILE;



#pragma warning(pop)
#pragma pack(pop)
#endif/* defined _NTFS_LAYOUT_H */