#include "ssufs-ops.h"

extern struct filehandle_t file_handle_array[MAX_OPEN_FILES];

int ssufs_allocFileHandle() {
	for(int i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_handle_array[i].inode_number == -1) {
			return i;
		}
	}
	return -1;
}

int ssufs_create(char *filename){	
	int inodenum;
	struct inode_t inode;
	// 이미 생성된 파일인지 확인
	if (open_namei(filename) > -1) {
		return -1;
	}
	// 비어 있는 inode 할당
	if((inodenum = ssufs_allocInode()) == -1) {
		return -1;
	}
	// inode 초기화
	inode.status = INODE_IN_USE;
	strcpy(inode.name, filename);
	inode.file_size = 0;
	memset(inode.direct_blocks, -1, sizeof(int) * MAX_FILE_SIZE);
	ssufs_writeInode(inodenum, &inode);
	return inodenum;
}

void ssufs_delete(char *filename){
	int inodenum, fd, i;
	struct inode_t inode;
	// inode 번호 불러오기
	if ((inodenum = open_namei(filename)) != -1) {
		ssufs_freeInode(inodenum);
	}
}

int ssufs_open(char *filename){
	int inodenum, fd, i = -1;
	struct inode_t inode;
	// inode 번호 불러오기
	if ((inodenum = open_namei(filename)) == -1) {
		return -1;
	}
	// 비어있는 filehandle 인덱스 찾기
	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (file_handle_array[i].inode_number == -1) {
			fd = i;
			break;
		}
	}
	// 비어있는 filehandle이 없음
	if (fd == -1) {
		return -1;
	}
	// file_handle_array를 설정하고 fd를 리턴
	else {
		file_handle_array[fd].inode_number = inodenum;
		return fd;
	}
}

void ssufs_close(int file_handle){
	file_handle_array[file_handle].inode_number = -1;
	file_handle_array[file_handle].offset = 0;
}

int ssufs_read(int file_handle, char *buf, int nbytes){
	struct filehandle_t *fh;
	struct inode_t inode;
	int blocknum, i, j, need_chunk_cnt;
	char read_data[BLOCKSIZE * MAX_FILE_SIZE + 1];
	char tmp[BLOCKSIZE + 1];
	memset(read_data, 0, BLOCKSIZE * MAX_FILE_SIZE + 1);
	memset(tmp, 0, BLOCKSIZE + 1);
	// fd는 최대 0 ~ 7까지 배정할 수 있음
	if (0 <= file_handle && file_handle >= MAX_OPEN_FILES) {
		return -1;
	}
	fh = &file_handle_array[file_handle];
	// 아직 할당되지 않은 fh인 경우 에러
	if (fh->inode_number == -1) {
		return -1;
	}
	// offset 끝에 도달함
	if (fh->offset == MAX_FILE_SIZE) {
		return -1;
	}
	// inode를 불러옴
	ssufs_readInode(fh->inode_number, &inode);
	/** chunk를 계산 **/
	// 최대 할당 크기인 256 bytes 보다 작아야함
	if (nbytes > BLOCKSIZE * MAX_FILE_SIZE) {
		return -1;
	}
	// 필요한 청크의 개수를 구함
	if (nbytes % BLOCKSIZE == 0) {
		need_chunk_cnt = nbytes / BLOCKSIZE;
	}
	else {
		need_chunk_cnt = nbytes / BLOCKSIZE + 1;
	}
	// 데이터 읽기
	for (i = 0; i < need_chunk_cnt; i++) {
		if (inode.direct_blocks[fh->offset] != -1) {
			ssufs_readDataBlock(inode.direct_blocks[fh->offset], tmp);
			strcat(read_data, tmp);
			ssufs_lseek(file_handle, 1);
		}
	}
	// nbytes 만큼 자름
	strncpy(buf, read_data, nbytes);
	buf[strlen(buf)] = 0;
	return 0;
}

int ssufs_write(int file_handle, char *buf, int nbytes){
	struct filehandle_t *fh;
	struct inode_t inode;
	int blocknum, i, j, need_chunk_cnt;
	char recovery[MAX_FILE_SIZE][BLOCKSIZE];
	int recovery_direct[MAX_FILE_SIZE];
	char tmp[BLOCKSIZE + 1];
	memset(tmp, 0, BLOCKSIZE);
	memset(recovery, 0, MAX_FILE_SIZE * BLOCKSIZE);
	// fd는 최대 0 ~ 7까지 배정할 수 있음
	if (0 <= file_handle && file_handle >= MAX_OPEN_FILES) {
		return -1;
	}
	fh = &file_handle_array[file_handle];
	// 아직 할당되지 않은 fh인 경우 에러
	if (fh->inode_number == -1) {
		return -1;
	}
	// offset 끝에 도달함
	if (fh->offset == MAX_FILE_SIZE) {
		return -1;
	}
	// inode를 불러옴
	ssufs_readInode(fh->inode_number, &inode);
	/** chunk를 계산 **/
	// 최대 할당 크기인 256 bytes 보다 작아야함
	if (nbytes > BLOCKSIZE * MAX_FILE_SIZE) {
		return -1;
	}
	// 필요한 청크의 개수를 구함
	if (nbytes % BLOCKSIZE == 0) {
		need_chunk_cnt = nbytes / BLOCKSIZE;
	}
	else {
		need_chunk_cnt = nbytes / BLOCKSIZE + 1;
	}
	// 실패할 경우 복구를 위해 copy
	for (i = 0; i < MAX_FILE_SIZE; i++) {
		if (inode.direct_blocks[i] != -1) {
			ssufs_readDataBlock(inode.direct_blocks[i], tmp);  
			strcpy(recovery[i], tmp);
			recovery_direct[i] = inode.direct_blocks[i];
		}
	}
	// 데이터 쓰기
	for (i = 0; i < need_chunk_cnt; i++) {
		// 블럭이 없는 경우 새로운 블록을 할당해야함
		if (inode.direct_blocks[fh->offset] == -1) {
			if ((blocknum = ssufs_allocDataBlock()) == -1) {
				// 데이터 블럭 복구
				memset(inode.direct_blocks, -1, MAX_FILE_SIZE);
				for (i = 0; i < MAX_FILE_SIZE; i++) {
					if (recovery[i][0] != 0) {
						inode.direct_blocks[i] = recovery_direct[i];
						ssufs_writeDataBlock(inode.direct_blocks[i], recovery[i]);
					}
				}
				return -1;
			}
			inode.direct_blocks[fh->offset] = blocknum;
		}
		// 덮어 쓰는 경우 기존 file_size를 감소시켜야함
		else {
			ssufs_readDataBlock(inode.direct_blocks[fh->offset], tmp);  
			inode.file_size -= strlen(tmp); 
		}
		strncpy(tmp, buf + BLOCKSIZE * i, BLOCKSIZE); // chunk Data 생성 
		tmp[BLOCKSIZE] = '\0';
		ssufs_writeDataBlock(inode.direct_blocks[fh->offset], tmp); // 스트링 복사
		inode.file_size += strlen(tmp);
		ssufs_writeInode(fh->inode_number, &inode); // inode 갱신
		ssufs_lseek(file_handle, 1);
	}
	return 0;
}

int ssufs_lseek(int file_handle, int nseek){
	int offset = file_handle_array[file_handle].offset;

	struct inode_t *tmp = (struct inode_t *) malloc(sizeof(struct inode_t));
	ssufs_readInode(file_handle_array[file_handle].inode_number, tmp);

	int fsize = tmp->file_size;

	offset += nseek;

	if ((fsize == -1) || (offset < 0) || (offset > fsize)) {
		free(tmp);
		return -1;
	}

	file_handle_array[file_handle].offset = offset;
	free(tmp);

	return 0;
}
