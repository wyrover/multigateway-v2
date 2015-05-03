//
//  db777.c
//  crypto777
//
//  Created by James on 4/9/15.
//  Copyright (c) 2015 jl777. All rights reserved.
//

#ifdef DEFINES_ONLY
#ifndef crypto777_db777_h
#define crypto777_db777_h
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "sophia.h"
#include "cJSON.h"
#include "system777.c"
#include "storage.c"

struct db777 { void *env,*ctl,*db,*asyncdb; char dbname[512],name[512],backupdir[512]; };

#define SOPHIA_USERDIR "/user"
struct db777 *db777_create(char *path,char *subdir,char *name,char *compression);
uint64_t db777_ctlinfo64(struct db777 *DB,char *field);
//void *db777_find(struct db777 *DB,void *key,int32_t keylen);
int32_t db777_add(int32_t forceflag,struct db777 *DB,void *key,int32_t keylen,void *value,int32_t len);
int32_t db777_addstr(struct db777 *DB,char *key,char *value);
int32_t db777_delete(struct db777 *DB,void *key,int32_t keylen);
int32_t db777_findstr(char *retbuf,int32_t max,struct db777 *DB,char *key);
void *db777_findM(int32_t *lenp,struct db777 *DB,void *key,int32_t keylen);
int32_t db777_close(struct db777 *DB);
int32_t db777_free(struct db777 *DB);
void **db777_copy_all(int32_t *nump,struct db777 *DB,char *field,int32_t size);
struct db777 *db777_getDB(char *dbname);
struct unspent_entries *db777_extract_unspents(uint32_t *nump,struct db777 *DB);

extern struct sophia_info SOPHIA;
extern struct db777 *DB_msigs,*DB_NXTaccts;//,*DB_NXTassettx,*DB_nodestats;

#endif
#else
#ifndef crypto777_db777_c
#define crypto777_db777_c

#ifndef crypto777_db777_h
#define DEFINES_ONLY
#include "db777.c"
#undef DEFINES_ONLY
#endif

void *db777_find(struct db777 *DB,void *key,int32_t keylen)
{
    void *obj,*result = 0;
    int32_t len;
    if ( DB == 0 || DB->db == 0 || (obj= sp_object(DB->db)) == 0 )
        return(0);
    if ( sp_set(obj,"key",key,keylen) == 0 )
        result = sp_get(DB->db,obj,&len);
    else sp_destroy(obj);
    return(result);
}

int32_t db777_delete(struct db777 *DB,void *key,int32_t keylen)
{
    void *obj;
    if ( DB == 0 || DB->db == 0 || (obj= sp_object(DB->db)) == 0 )
        return(0);
    if ( sp_set(obj,"key",key,keylen) == 0 )
        return(sp_delete(DB->db,obj));
    else sp_destroy(obj);
    return(-1);
}

int32_t db777_add(int32_t forceflag,struct db777 *DB,void *key,int32_t keylen,void *value,int32_t len)
{
    void *obj,*val;
    int32_t allocsize;
    if ( DB == 0 || DB->db == 0 )
        return(0);
    if ( forceflag == 0 && (obj= db777_find(DB,key,keylen)) != 0 )
    {
        if ( (val= sp_get(obj,"value",&allocsize)) != 0 )
        {
            if ( allocsize == len && memcmp(val,value,len) == 0 )
            {
                sp_destroy(obj);
                return(0);
            }
        }
        sp_destroy(obj);
    }
    if ( (obj= sp_object(DB->db)) == 0 )
        return(-1);
    if ( sp_set(obj,"key",key,keylen) != 0 || sp_set(obj,"value",value,len) != 0 )
    {
        sp_destroy(obj);
        return(-1);
    }
    //printf("DB.%p add.[%p %d] val.%p %d [crcs %d %d]\n",DB,key,keylen,value,len,_crc32(0,key,keylen),_crc32(0,value,len));
    return(sp_set(DB->db,obj));
}

int32_t db777_addstr(struct db777 *DB,char *key,char *value)
{
    return(db777_add(0,DB,key,(int32_t)strlen(key)+1,value,(int32_t)strlen(value)+1));
}

int32_t db777_findstr(char *retbuf,int32_t max,struct db777 *DB,char *key)
{
    void *obj,*val;
    int32_t valuesize = -1;
    if ( (obj= db777_find(DB,key,(int32_t)strlen(key)+1)) != 0 )
    {
        if ( (val= sp_get(obj,"value",&valuesize)) != 0 )
        {
            max--;
            memcpy(retbuf,val,(valuesize < max) ? valuesize : max), retbuf[max] = 0;
        } else retbuf[0] = 0;
        sp_destroy(obj);
    }
    return(valuesize);
}

void *db777_findM(int32_t *lenp,struct db777 *DB,void *key,int32_t keylen)
{
    void *obj,*val,*ptr = 0;
    int32_t valuesize = -1;
    if ( (obj= db777_find(DB,key,keylen)) != 0 )
    {
       // printf("found keylen.%d\n",keylen);
        if ( (val= sp_get(obj,"value",&valuesize)) != 0 )
        {
            ptr = calloc(1,valuesize);
            memcpy(ptr,val,valuesize);
            *lenp = valuesize;
        }
        sp_destroy(obj);
    }
    return(ptr);
}

uint64_t db777_ctlinfo64(struct db777 *DB,char *field)
{
	void *obj,*ptr; uint64_t val = 0;
    
    if ( (obj= sp_get(DB->ctl,field)) != 0 )
    {
        if ( (ptr= sp_get(obj,"value",NULL)) != 0 )
            val = calc_nxt64bits(ptr);
        sp_destroy(obj);
    }
    return(val);
}

void **db777_copy_all(int32_t *nump,struct db777 *DB,char *field,int32_t size)
{
    void *obj,*cursor,*value,*ptr,**ptrs = 0;
    int32_t len,max,n = 0;
    *nump = 0;
    max = 100;
    if ( DB == 0 || DB->db == 0 )
        return(0);
    obj = sp_object(DB->db);
    if ( (cursor= sp_cursor(DB->db,obj)) != 0 )
    {
        ptrs = (void **)calloc(sizeof(void *),max+1);
        while ( (obj= sp_get(cursor,obj)) != 0 )
        {
            value = sp_get(obj,field,&len);
            if ( len > 0 && (size == 0 || len == size) )
            {
                ptr = malloc(len);
                memcpy(ptr,value,len);
                //printf("%p set [%d] <- %p.len %d\n",ptrs,n,ptr,len);
                ptrs[n++] = ptr;
                if ( n >= max )
                {
                    max = n + 100;
                    ptrs = (void **)realloc(ptrs,sizeof(void *)*(max+1));
                }
            } //else printf("wrong size.%d\n",len);
        }
        sp_destroy(cursor);
    }
    if ( ptrs != 0 )
        ptrs[n] = 0;
    *nump = n;
    printf("ptrs.%p [0] %p numdb.%d\n",ptrs,ptrs[0],n);
    return(ptrs);
}

#include "uthash.h"

struct unspent_entry unspent_entry(struct unspent_output *U)
{
    struct unspent_entry E;
    memset(&E,0,sizeof(E));
    E.unspentind = U->B.rawind;
    E.amount = U->value;
    return(E);
}

int32_t update_unspent_entries(struct unspent_entries *unspents,struct unspent_output *U)
{
    uint32_t i,n,unspentind,width; struct unspent_entry *entries = unspents->entries;
    if ( unspents->addrind == U->addrind )
    {
        if ( (n= unspents->count) > 0 )
        {
            if ( entries == 0 )
            {
                printf("unexpected null entries???\n");
                return(-1);
            }
            unspentind = U->B.rawind;
            for (i=0; i<n; i++)
            {
                if ( entries[i].unspentind == unspentind )
                {
                    if ( U->B.spent == 0 )
                        printf("error: found matched unspentind.%u in slot.[%d] when new one is not spent?\n",unspentind,i);
                    else
                    {
                        entries[i] = entries[--unspents->count];
                        memset(&entries[unspents->count],0,sizeof(entries[unspents->count]));
                        printf("found matched unspentind.%u in slot.[%d] max.%d count.%d\n",unspentind,i,unspents->max,unspents->count);
                        return(i);
                    }
                }
            }
        }
        if ( unspents->count >= unspents->max )
        {
            width = ((unspents->count+1) << 1);
            if ( width > 256 )
                width = 256;
            unspents->max = (unspents->count + width);
            unspents->entries = entries = realloc(entries,sizeof(*entries) * unspents->max);
            entries[unspents->count++] = unspent_entry(U);
            //printf("new max.%d count.%d\n",unspents->max,unspents->count);
            return(unspents->count);
        }
    } else printf("update_unspent_entries: addrind mismatch %u vs %u\n",unspents->addrind,U->addrind);
    return(0);
}

struct unspent_entries *db777_extract_unspents(uint32_t *nump,struct db777 *DB)
{
    int32_t keylen,valuelen,n,m,total,numaddrs,addrind;
    void *obj,*cursor; struct unspent_output *U; struct address_entry *key,*value; struct unspent_entries *ptr,*table = 0;
    if ( DB == 0 || DB->db == 0 )
        return(0);
    numaddrs = total = n = m = 0;
    obj = sp_object(DB->db);
    if ( (cursor= sp_cursor(DB->db,obj)) != 0 )
    {
        while ( (obj= sp_get(cursor,obj)) != 0 )
        {
            total++;
            key = sp_get(obj,"key",&keylen);
            value = sp_get(obj,"value",&valuelen);
            if ( key->rawind == value->rawind )
            {
                if ( keylen == sizeof(uint32_t) && valuelen == sizeof(struct unspent_output) )
                {
                    U = (struct unspent_output *)value, addrind = U->addrind;
                    if ( U->value == 0 || U->txidind == 0 || (rand() % 1000) == 0 )
                        printf("m.%d n.%d total.%d unspentind.%u addrind.%u block.%u %d_v%d %.8f key (%p).%d value (%p).%d | spent.%d txidind.%u v%d\n",m,n,total,U->B.rawind,U->addrind,U->B.blocknum,U->B.txind,U->B.v,dstr(U->value),key,keylen,value,valuelen,U->B.spent,U->spend_txidind,U->spend_vout);
                    HASH_FIND(hh,table,&addrind,sizeof(addrind),ptr);
                    if ( ptr == 0 )
                    {
                        ptr = calloc(1,sizeof(*ptr));
                        ptr->addrind = addrind;
                        HASH_ADD(hh,table,addrind,sizeof(addrind),ptr);
                        numaddrs++;
                    }
                    update_unspent_entries(ptr,(struct unspent_output *)value);
                    if ( ptr->max < ptr->count )
                        printf("illegal max.%d vs count.%d\n",ptr->max,ptr->count);
                    n++;
                }
                else if ( keylen == sizeof(struct address_entry) && valuelen == sizeof(uint32_t) )
                    m++;
            } else printf("unexpected mismatch of unspentinds (%d).%d vs (%d).%d\n",key->rawind,keylen,value->rawind,valuelen);
        }
    }
    sp_destroy(cursor);
    printf("n.%d m.%d numaddrs.%d\n",n,m,numaddrs);
    *nump = numaddrs;
    return(table);
}

#endif
#endif