#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <assert.h>

struct Buffer
{
	size_t size;
	void *memory;
	size_t taken;
};
struct Buffer Buffer_new(size_t size)
{
	struct Buffer buffer;
	buffer.size = size;
	buffer.taken = 0;
	buffer.memory = malloc(size);

	return buffer;
}
void Buffer_readFromFile(struct Buffer *buffer, FILE *handle)
{
	assert(handle != NULL);
	assert(buffer->taken == 0);

	buffer->taken = fread(buffer->memory, 1, buffer->size, handle);
}
void Buffer_dump(struct Buffer *buffer)
{
	buffer->taken = 0;
}
void Buffer_zero(struct Buffer *buffer)
{
	bzero(buffer->memory, buffer->size);
}
/* XOR Buffers, storing the result in @left buffer */
void Buffer_xor_uint64(struct Buffer *left, struct Buffer *right)
{
	uint64_t chunk;
	size_t chunkSize = sizeof(chunk);

	assert(left != NULL && right != NULL);
	assert(left->size == right->size);
	assert(left->size % chunkSize == 0);

	uint64_t *leftMem = (uint64_t *) left->memory;
	uint64_t *rightMem = (uint64_t *) right->memory;

	size_t chunkCount = left->size / chunkSize;
	unsigned int chunkIndex;
	for (chunkIndex=0; chunkIndex<chunkCount; chunkIndex++)
	{
		leftMem[chunkIndex] ^= rightMem[chunkIndex];
	}
}
size_t Buffer_writeToFile(struct Buffer *buffer, FILE *handle)
{
	return fwrite(buffer->memory, 1, buffer->taken, handle);
}
void Buffer_free(struct Buffer *buffer)
{
	free(buffer->memory);
	buffer->memory = NULL;
}

struct File
{
	char *name;
	FILE *handle;
	struct Buffer buffer;
};

size_t bufferSize = sizeof(uint64_t) * 131072; // 131072 * 8 = 1 MB

int main(int argc, char **argv)
{
	unsigned int fileCount = argc-1;
	struct File *inputFiles = NULL;
	FILE *outputFileHandle = (FILE *) stdout;
	struct Buffer outputBuffer = Buffer_new(bufferSize);
	int cleanupAndReturn (int returnCode)
	{
		// Clean input files
		if (inputFiles != NULL)
		{
			int fileIndex;
			struct File file;
			for (fileIndex=0; fileIndex < fileCount; fileIndex++)
			{
				file = inputFiles[fileIndex];

				// Close file handle
				if (file.handle != NULL)
				{
					fclose(file.handle);
				}

				// Free buffer memory
				Buffer_free(&(file.buffer));
			}

			// Free File structures
			free(inputFiles);
		}

		// Close output file
		fclose(outputFileHandle);

		// Free output buffer
		Buffer_free(&outputBuffer);

		return returnCode;
	}

	/* Check output buffer creation */
        if (outputBuffer.memory == NULL)
        {
                fprintf(stderr, "Unable to allocate %i bytes of memory for output buffer\n", bufferSize);
                return cleanupAndReturn(4);
        }

	/* Check parameters */
	if (fileCount < 1)
	{
		fprintf(stderr, "At least 1 file are needed on input\n");
		return cleanupAndReturn(1);
	}

	/* Allocate memory for File structures */
	inputFiles = (struct File *) calloc(fileCount, sizeof(struct File));
	if (inputFiles == NULL)
	{
		fprintf(stderr, "Unable to allocate memory for %i internal file structures\n", fileCount);
		return cleanupAndReturn(1);
	}

	/* Prepare File structures */
	{
		struct File *currentFile;
		unsigned int fileIndex;
		FILE *newHandle;
		void *newBufferSpace;

                for (fileIndex=0; fileIndex < fileCount; fileIndex++)
                {
			currentFile = inputFiles + fileIndex; // pointer arithmetics

			// Set name
			currentFile->name = argv[fileIndex + 1];

			// Open file
			if (strcmp(currentFile->name, "-") == 0)
			{
				newHandle = (FILE *) stdin;
			}
			else
			{
				newHandle = fopen(currentFile->name, "r");
			}

			if (newHandle == NULL)
			{
				fprintf(stderr, "Unable to open file '%s': ", currentFile->name);
				perror(NULL);
				return cleanupAndReturn(2);
			}
			else
			{
				currentFile->handle = newHandle;
			}

			// Allocate buffer
                        currentFile->buffer = Buffer_new(bufferSize);
			if (currentFile->buffer.memory == NULL)
                        {
                                fprintf(stderr, "Unable to allocate %i bytes of memory for input buffer #%i\n", bufferSize, fileIndex);
                                return cleanupAndReturn(3);
                        }
			else
			{
				// Do nothing
			}
                }
	}

	/* Debug printing */
	if (0)
	{
	fprintf(stderr, "Prepared %i input files\n", fileCount);
	{
		unsigned int fileIndex;
		for (fileIndex=0; fileIndex < fileCount; fileIndex++)
		{
			fprintf(stderr, "File %02i: Name: '%s'\n", fileIndex, inputFiles[fileIndex].name);
			fprintf(stderr, "       : File handle: 0x%X\n", inputFiles[fileIndex].handle);
			fprintf(stderr, "       : Buffer:      0x%X, %i bytes\n", inputFiles[fileIndex].buffer.memory, inputFiles[fileIndex].buffer.size);
			fprintf(stderr, "\n");
		}
	}
	}

	/* Read & Xor */
	{
		unsigned int fileIndex;
		struct File *file;
		size_t correctReadBytes;
		char allEof;

		while (1)
		{
			/* Read from all files */
                	for (fileIndex=0; fileIndex < fileCount; fileIndex++)
	                {
				file = inputFiles + fileIndex;

				// Read
				Buffer_readFromFile(&(file->buffer), file->handle);

				// Check for read error
				if (ferror(file->handle))
				{
					fprintf(stderr, "Error %i reading '%s'\n", ferror(file->handle), file->name);
					return cleanupAndReturn(5);
				}
			}

			/* Check that we read same amount of bytes from each file
			   (full buffer or less) */
			correctReadBytes = inputFiles[0].buffer.taken;
                        for (fileIndex=0; fileIndex < fileCount; fileIndex++)
                        {
				if (inputFiles[fileIndex].buffer.taken != correctReadBytes)
				{
					fprintf(stderr, "Got %i bytes of data from file '%s' whereas other file(s) produced %i bytes.\n", inputFiles[fileIndex].buffer.taken, inputFiles[fileIndex].name, correctReadBytes);
					fprintf(stderr, "That means one file is shorter than others, and that's an error.\n");
					return cleanupAndReturn(6);
				}
				else
				{
					// Do nothing
				}
			}

			/* Xor */
			Buffer_zero(&outputBuffer);
			outputBuffer.taken = correctReadBytes;
                        for (fileIndex=0; fileIndex < fileCount; fileIndex++)
			{
				Buffer_xor_uint64(&outputBuffer, &(inputFiles[fileIndex].buffer));
			}

			/* Write output buffer */
			Buffer_writeToFile(&outputBuffer, outputFileHandle);

			/* Check writing */
			if (ferror(outputFileHandle))
			{
				fprintf(stderr, "Error writing to output file: ");
				perror(NULL);
				return cleanupAndReturn(7);
			}

			/* Clear all buffers */
			for (fileIndex=0; fileIndex < fileCount; fileIndex++)
                        {
				Buffer_dump(&(inputFiles[fileIndex].buffer));
			}

			/* Check EOF on all files
			   If not all files are EOF, short file detection logic will stop the programm */
			allEof = 1;
			for (fileIndex=0; fileIndex < fileCount; fileIndex++)
                        {
				allEof = allEof && feof(inputFiles[fileIndex].handle);
			}

			if (allEof)
			{
				// fprintf(stderr, "All files on EOF.\n");
				break;
			}
		}
	}





	return cleanupAndReturn(0);
}
