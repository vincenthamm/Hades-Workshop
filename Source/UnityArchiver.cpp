#include "UnityArchiver.h"

bool HasFileTypeName(uint32_t type) {
	return type==21 || type==28 || type==43 || type==48 || type==49 || type==109 || type==115 || type==213;
}

int UnityArchiveMetaData::Load(fstream& f) {
	unsigned int i,j;
	string mgc = "";
	uint32_t archivestart;
	for (i=0;i<8;i++)
		mgc.push_back(f.get());
	if (mgc.compare("UnityRaw")==0) {
		archivestart = 0x70;
		archive_type = 1;
	} else {
		archivestart = 0;
		archive_type = 0;
	}
	f.seekg(archivestart);
	header_size = ReadLongBE(f);
	header_file_size = ReadLongBE(f);
	header_id = ReadLongBE(f);
	header_file_offset = ReadLongBE(f);
	header_unknown1 = ReadLongBE(f);
	if (header_id!=0x0F)
		return 1;
	header_version = "";
	for (i=0;i<8;i++)
		header_version.push_back(f.get());
	header_unknown2 = ReadLong(f);
	header_unknown3 = f.get();
	header_file_info_amount = ReadLong(f);
	file_info_type = new int32_t[header_file_info_amount];
	if (header_unknown2==0x5) {
		file_info_unkstruct_amount = new uint32_t[header_file_info_amount];
		file_info_unkstruct_text_size = new uint32_t[header_file_info_amount];
	}
	for (i=0;i<header_file_info_amount;i++) {
		file_info_type[i] = ReadLong(f);
		if (file_info_type[i]>=0)
			f.seekg(0x10,ios::cur);
		else
			f.seekg(0x20,ios::cur);
		if (header_unknown2==0x5) {
			file_info_unkstruct_amount[i] = ReadLong(f);
			file_info_unkstruct_text_size[i] = ReadLong(f);
			f.seekg(file_info_unkstruct_amount[i]*0x18+file_info_unkstruct_text_size[i],ios::cur);
		}
	}
	header_file_amount = ReadLong(f);
	f.seekg(GetAlignOffset(f.tellg()),ios::cur);
	file_info = new uint64_t[header_file_amount];
	file_offset_start = new uint32_t[header_file_amount];
	file_size = new uint32_t[header_file_amount];
	file_type1 = new uint32_t[header_file_amount];
	file_type2 = new uint32_t[header_file_amount];
	file_unknown2 = new uint32_t[header_file_amount];
	file_name_len = new uint32_t[header_file_amount];
	file_name = new string[header_file_amount];
	for (i=0;i<header_file_amount;i++) {
		file_info[i] = ReadLongLong(f);
		file_offset_start[i] = ReadLong(f);
		file_size[i] = ReadLong(f);
		file_type1[i] = ReadLong(f);
		file_type2[i] = ReadLong(f);
		file_unknown2[i] = ReadLong(f);
		if (HasFileTypeName(file_type1[i])) {
			size_t curpos = f.tellg();
			f.seekg(archivestart+header_file_offset+file_offset_start[i]);
			file_name_len[i] = ReadLong(f);
			file_name[i] = "";
			for (j=0;j<file_name_len[i];j++)
				file_name[i].push_back(f.get());
			f.seekg(curpos);
		} else {
			file_name_len[i] = 0;
			file_name[i] = "";
		}
	}
	return 0;
}

uint32_t UnityArchiveMetaData::GetFileOffset(string filename, uint32_t filetype, unsigned int num, string folder) {
	int i = GetFileIndex(filename,filetype,num,folder);
	if (i>=0)
		return GetFileOffsetByIndex(i);
	return 0;
}

uint32_t UnityArchiveMetaData::GetFileOffsetByInfo(uint64_t info, uint32_t filetype, string folder) {
	int i = GetFileIndexByInfo(info,filetype,folder);
	if (i>=0)
		return GetFileOffsetByIndex(i);
	return 0;
}

uint32_t UnityArchiveMetaData::GetFileOffsetByIndex(unsigned int fileid, string folder) {
	if (fileid>=header_file_amount)
		return 0;
	uint32_t res = archive_type==1 ? 0x70 : 0;
	res += header_file_offset+file_offset_start[fileid];
	if (HasFileTypeName(file_type1[fileid]))
		res += file_name_len[fileid]+GetAlignOffset(file_name_len[fileid])+4;
	return res;
}

int32_t UnityArchiveMetaData::GetFileIndex(string filename, uint32_t filetype, unsigned int num, string folder) {
	unsigned int i;
	for (i=0;i<header_file_amount;i++)
		if (file_name[i].compare(filename)==0 && (filetype==0xFFFFFFFF || filetype==file_type1[i])) {
			if (num==0)
				return i;
			else
				num--;
		}
	return -1;
}

int32_t UnityArchiveMetaData::GetFileIndexByInfo(uint64_t info, uint32_t filetype, string folder) {
	unsigned int i;
	for (i=0;i<header_file_amount;i++)
		if (file_info[i]==info && (filetype==0xFFFFFFFF || filetype==file_type1[i]))
			return i;
	return -1;
}

uint32_t* UnityArchiveMetaData::Duplicate(fstream& fbase, fstream& fdest, bool* copylist, uint32_t* filenewsize) {
	uint32_t archivestart = archive_type==1 ? 0x70 : 0;
	uint32_t* res = new uint32_t[header_file_amount];
	uint32_t copysize, offstart, offtmp, filenewsizetmp;
	unsigned int i,j;
	char* buffer;
	copysize = archivestart+0x25; // up to file_info data
	for (i=0;i<header_file_info_amount;i++) {
		if (file_info_type[i]>=0)
			copysize += 0x14;
		else
			copysize += 0x24;
		if (header_unknown2==0x5)
			copysize += 0x18*file_info_unkstruct_amount[i]+file_info_unkstruct_text_size[i]+8;
	}
	buffer = new char[copysize];
	fbase.read(buffer,copysize);
	fdest.write(buffer,copysize);
	delete[] buffer;
	WriteLong(fdest,header_file_amount);
	fdest.seekp(GetAlignOffset(fdest.tellp()),ios::cur);
	offstart = 0;
	for (i=0;i<header_file_amount;i++) {
		filenewsizetmp = filenewsize[i];
		WriteLongLong(fdest,file_info[i]);
		WriteLong(fdest,offstart);
		if (copylist[i]) {
			WriteLong(fdest,file_size[i]);
		} else {
			if (HasFileTypeName(file_type1[i]))
				filenewsizetmp += 4+file_name_len[i]+GetAlignOffset(file_name_len[i]);
			filenewsizetmp++; // DEBUG : text files and some others use this kind of padding but not all files do
			filenewsizetmp += GetAlignOffset(filenewsizetmp,8);
			WriteLong(fdest,filenewsizetmp);
		}
		WriteLong(fdest,file_type1[i]);
		WriteLong(fdest,file_type2[i]);
		WriteLong(fdest,file_unknown2[i]);
		offtmp = fdest.tellp();
		fdest.seekp(archivestart+header_file_offset+offstart);
		if (HasFileTypeName(file_type1[i])) {
			WriteLong(fdest,file_name_len[i]);
			for (j=0;j<file_name_len[i];j++)
				fdest.put(file_name[i].at(j));
			fdest.seekp(GetAlignOffset(fdest.tellp()),ios::cur);
		}
		res[i] = fdest.tellp();
		if (copylist[i]) {
			copysize = file_size[i];
			if (HasFileTypeName(file_type1[i]))
				copysize -= 4+file_name_len[i]+GetAlignOffset(file_name_len[i]);
			fbase.seekg(GetFileOffsetByIndex(i));
			buffer = new char[copysize];
			fbase.read(buffer,copysize);
			fdest.write(buffer,copysize);
			delete[] buffer;
		}
		fdest.seekp(offtmp);
		if (copylist[i])
			offstart += file_size[i];
		else
			offstart += filenewsizetmp;
		offstart += GetAlignOffset(offstart,8);
	}
	if (offtmp<archivestart+header_file_offset) {
		copysize = archivestart+header_file_offset-offtmp;
		fbase.seekg(offtmp);
		buffer = new char[copysize];
		fbase.read(buffer,copysize);
		fdest.write(buffer,copysize);
		delete[] buffer;
	}
	fdest.seekp(0,ios::end);
	while (fdest.tellp()%4!=0)
		fdest.put(0);
	uint32_t fdestsize = fdest.tellp();
	if (archive_type==0) {
		fdest.seekp(4);
		WriteLongBE(fdest,fdestsize);
	} else {
		uint32_t size;
		fdest.seekp(0x1B);
		WriteLongBE(fdest,fdestsize);
		fdest.seekp(0x2B);
		size = fdestsize-0x3C;
		WriteLongBE(fdest,size);
		WriteLongBE(fdest,size);
		WriteLongBE(fdest,fdestsize);
		fdest.seekp(0x69);
		size = fdestsize-archivestart;
		WriteLongBE(fdest,size);
		fdest.seekp(archivestart+4);
		WriteLongBE(fdest,size);
	}
	return res;
}

int UnityArchiveIndexListData::Load(fstream& f) {
	uint32_t fnamelen;
	amount = ReadLong(f);
	path = new string[amount];
	unk1 = new uint32_t[amount];
	index = new uint32_t[amount];
	unk2 = new uint32_t[amount];
	unsigned int i,j;
	for (i=0;i<amount;i++) {
		fnamelen = ReadLong(f);
		path[i] = "";
		for (j=0;j<fnamelen;j++)
			path[i].push_back(f.get());
		f.seekg(GetAlignOffset(fnamelen),ios::cur);
		unk1[i] = ReadLong(f);
		index[i] = ReadLong(f);
		unk2[i] = ReadLong(f);
	}
	return 0;
}

uint32_t UnityArchiveIndexListData::GetFileIndex(string filepath) {
	for (unsigned int i=0;i<amount;i++)
		if (filepath.compare(path[i])==0)
			return index[i];
	return 0;
}

int UnityArchiveAssetBundle::Load(fstream& f) {
	uint32_t fnamelen;
	uint32_t unkinfoamount;
	ReadLong(f);
	unkinfoamount = ReadLong(f);
	f.seekg(unkinfoamount*12,ios::cur);
	amount = ReadLong(f);
	path = new string[amount];
	index = new uint32_t[amount];
	unk1 = new uint32_t[amount];
	unk2 = new uint32_t[amount];
	info = new uint64_t[amount];
	unsigned int i,j;
	for (i=0;i<amount;i++) {
		fnamelen = ReadLong(f);
		path[i] = "";
		for (j=0;j<fnamelen;j++)
			path[i].push_back(f.get());
		f.seekg(GetAlignOffset(fnamelen),ios::cur);
		index[i] = ReadLong(f);
		unk1[i] = ReadLong(f);
		unk2[i] = ReadLong(f);
		info[i] = ReadLong(f);
		info[i] |= ((uint64_t)ReadLong(f) << 32);
	}
	return 0;
}

uint32_t UnityArchiveAssetBundle::GetFileIndex(string filepath) {
	for (unsigned int i=0;i<amount;i++)
		if (filepath.compare(path[i])==0)
			return index[i];
	return 0;
}

uint64_t UnityArchiveAssetBundle::GetFileInfo(string filepath) {
	for (unsigned int i=0;i<amount;i++)
		if (filepath.compare(path[i])==0)
			return info[i];
	return 0;
}
