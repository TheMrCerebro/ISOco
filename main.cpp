/*
 *
 * Copyright (c) 2022 TheMrCerebro
 *
 * ISOco - Zlib license.
 *
 * This software is provided 'as-is', without any express or
 * implied warranty. In no event will the authors be held
 * liable for any damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute
 * it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented;
 * you must not claim that you wrote the original software.
 * If you use this software in a product, an acknowledgment
 * in the product documentation would be appreciated but
 * is not required.
 *
 * 2. Altered source versions must be plainly marked as such,
 * and must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any
 * source distribution.
 *
*/

#include <iostream>
#include <string>
#include <string.h>
#include <algorithm>

#include "zlib/zlib.h"
#include "zlib/zconf.h"

using namespace std;

typedef struct ciso_header
{
	unsigned char magic[4];
	unsigned int header_size;
	unsigned __int64 total_bytes;
	unsigned int block_size;
	unsigned char ver;
	unsigned char align;
	unsigned char rsv_06[2];
}CISO_H;

string ReplaceAll(string str, const string& from, const string& to)
{
    size_t start_pos = 0;
    while((start_pos = str.find(from, start_pos)) != string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
    return str;
}

int main()
{
    cout << "                ___   _______  _______  _______  _______ " << endl;
    cout << "               |   | |       ||       ||       ||       |" << endl;
    cout << "               |   | |  _____||   _   ||       ||   _   |" << endl;
    cout << "               |   | | |_____ |  | |  ||       ||  | |  |" << endl;
    cout << "               |   | |_____  ||  |_|  ||      _||  |_|  |" << endl;
    cout << "               |   |  _____| ||       ||     |_ |       |" << endl;
    cout << "               |___| |_______||_______||_______||_______|" << endl;
    cout << " (o)================================================================(o)" << endl;
    cout << "                - PSP ISO COMPRESSOR [by TheMrCerebro] -" << endl;
    cout << " (o)================================================================(o)" << endl;
    cout << endl;
    cout << "   With this tool you can compress a game in iso format used by a PSP" << endl;
    cout << "   with CFW. Simply indicate the name or path of the file to compress" << endl;
    cout << "     and voila!, the ISO will be compressed as much as possible." << endl;
    cout << endl;

    cout << " File: ";

    string path;
    getline(cin,path);

    // Elimina las comillas (") de la ruta del archivo
    path.erase(std::remove(path.begin(), path.end(), '\"'), path.end());

    // Si la extension del archivo esta en mayusculas la cambia a minusculas.
    if(path.find(".ISO") != std::string::npos)
        path = ReplaceAll(path,".ISO",".iso");

    if(path.find(".iso") < 0)
        cout << " Is not a ISO image" << endl;

    cout << endl;

    FILE* fin = fopen(path.c_str(), "rb");

	path = ReplaceAll(path,".iso",".cso");

    FILE* fout = fopen(path.c_str(), "wb");

	fseek(fin,0,SEEK_END);

	unsigned __int64 file_size = ftell(fin);

	/* init ciso header */
    CISO_H ciso;
	memset(&ciso, 0, sizeof(ciso));
	ciso.magic[0] = 'C';
	ciso.magic[1] = 'I';
	ciso.magic[2] = 'S';
	ciso.magic[3] = 'O';
	ciso.ver = 0x01;
	ciso.block_size = 0x800; /* ISO9660 one of sector */
	ciso.total_bytes = file_size;

    int ciso_total_block = (int)file_size / ciso.block_size;

	fseek(fin, 0, SEEK_SET);

	/* allocate index block */
	int index_size = (ciso_total_block + 1 ) * sizeof(unsigned int);

	unsigned int *index_buf  = new unsigned int[index_size];
	unsigned int *crc_buf    = new unsigned int[index_size];
	unsigned char *block_buf1 = new unsigned char[ciso.block_size];
	unsigned char *block_buf2 = new unsigned char[ciso.block_size*2];

	/* write header block */
	fwrite(&ciso, 1, sizeof(ciso), fout);

	/* dummy write index block */
	unsigned char *buf4 = new unsigned char[64];
	fwrite(index_buf, 1, index_size, fout);

	unsigned __int64 write_pos = sizeof(ciso) + index_size;

	/* init zlib */
	z_stream z;
	z.zalloc = 0;
	z.zfree  = 0;
	z.opaque = 0;

	/* compress data */
	int percent_period = ciso_total_block / 100;
	int percent_cnt = ciso_total_block / 100;

	int align_b = 1 << (ciso.align);
	int align_m = align_b - 1;

	int block;
	for(block = 0; block < ciso_total_block ; block++)
	{
		if(--percent_cnt <= 0)
		{
			percent_cnt = percent_period;
			cout << " Compressing " << block / percent_period << "%\r";
		}

		deflateInit2(&z, 9, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);

		/* write align */
		int align = (int)write_pos & align_m;
		if(align)
		{
			align = align_b - align;
			fwrite(buf4, 1, align, fout);
			write_pos += align;
		}

		/* mark offset index */
		index_buf[block] = write_pos >> (ciso.align);

		/* read buffer */
		z.next_out  = block_buf2;
		z.avail_out = ciso.block_size*2;
		z.next_in   = block_buf1;
		z.avail_in  = fread(block_buf1, 1, ciso.block_size, fin);

		/* compress block */
		deflate(&z, Z_FINISH);

		unsigned int cmp_size = ciso.block_size * 2 - z.avail_out;
		if(cmp_size >= ciso.block_size)
		{
			cmp_size = ciso.block_size;
			memcpy(block_buf2, block_buf1, cmp_size);
			index_buf[block] |= 0x80000000;
		}

		/* write compressed block */
		fwrite(block_buf2, 1, cmp_size, fout);

		/* mark next index */
		write_pos += cmp_size;

		/* term zlib */
		deflateEnd(&z);
	}

	/* last position (total size)*/
	index_buf[block] = write_pos >> (ciso.align);

	/* write header & index block */
	fseek(fout, sizeof(ciso), SEEK_SET);
	fwrite(index_buf, 1, index_size, fout);

	cout << " Compression completed!" << endl;

	delete [] index_buf;
	delete [] crc_buf;
	delete [] block_buf1;
	delete [] block_buf2;

	fclose(fin);
	fclose(fout);

    cout << endl;
	system("pause");
    return 0;
}
