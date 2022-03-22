/*
Copyright (c) 2015-20 Mike Brady

A text-only sample metadata player for Shairport Sync

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/



#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <arpa/inet.h>


// From Stack Overflow, with thanks:
// http://stackoverflow.com/questions/342409/how-do-i-base64-encode-decode-in-c
// minor mods to make independent of C99.
// more significant changes make it not malloc memory
// needs to initialise the docoding table first

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static int decoding_table[256]; // an incoming char can range over ASCII, but by mistake could be all 8 bits.

static int mod_table[] = {0, 2, 1};

void initialise_decoding_table() {
    int i;
    for (i = 0; i < 64; i++)
        decoding_table[(unsigned char) encoding_table[i]] = i;
}

// pass in a pointer to the data, its length, a pointer to the output buffer and a pointer to an int containing its maximum length
// the actual length will be returned.

char *base64_encode(const unsigned char *data,
                    size_t input_length,
                    char *encoded_data,
                    size_t *output_length) {

    size_t calculated_output_length = 4 * ((input_length + 2) / 3);
    if (calculated_output_length> *output_length)
      return(NULL);
    *output_length = calculated_output_length;

    int i,j;
    for (i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    return encoded_data;
}


// pass in a pointer to the data, its length, a pointer to the output buffer and a pointer to an int containing its maximum length
// the actual length will be returned.
int base64_decode(const char *data,
                             size_t input_length,
                             unsigned char *decoded_data,
                             size_t *output_length) {

    //remember somewhere to call initialise_decoding_table();

    if (input_length % 4 != 0) return -1;

    size_t calculated_output_length = input_length / 4 * 3;
    if (data[input_length - 1] == '=') calculated_output_length--;
    if (data[input_length - 2] == '=') calculated_output_length--;
    if (calculated_output_length> *output_length)
      return(-1);
    *output_length = calculated_output_length;

    int i,j;
    for (i = 0, j = 0; i < input_length;) {

        uint32_t sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        uint32_t sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        uint32_t triple = (sextet_a << 3 * 6)
        + (sextet_b << 2 * 6)
        + (sextet_c << 1 * 6)
        + (sextet_d << 0 * 6);

        if (j < *output_length) decoded_data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length) decoded_data[j++] = (triple >> 0 * 8) & 0xFF;
    }

    return 0;
}

int main(void) {
  fd_set rfds;
  int retval;

  initialise_decoding_table();

  while (1) {
    char str[1025];
    if (fgets (str, 1024, stdin)) {
      uint32_t type,code,length;
      char tagend[1024];
      int ret = sscanf(str,"<item><type>%8x</type><code>%8x</code><length>%u</length>",&type,&code,&length);
      if (ret==3) {
        // now, think about processing the tag.
        // basically, we need to get hold of the base-64 data, if any
        size_t outputlength=0;
        char payload[32769];
        if (length>0) {
          // get the next line, which should be a data tag
          char datatagstart[64],datatagend[64];
          memset(datatagstart,0,64);
          int rc = fscanf(stdin,"<data encoding=\"base64\">");
          if (rc==0) {
            // now, read in that big (possibly) base64 buffer
            int c = fgetc(stdin);
            uint32_t b64size = 4*((length+2)/3);
            char * b64buf = malloc(b64size+1);
            memset(b64buf,0,b64size+1);
            if (b64buf) {
              if (fgets(b64buf, b64size+1, stdin)!=NULL) {
                // it looks like we got it
                // printf("Looks like we got it, with a buffer size of %u.\n",b64size);
                //puts(b64buf);
                //printf("\n");
                // now, if it's not a picture, let's try to decode it.
                if (code!='PICT') {
                  int inputlength=32768;
                  if (b64size<inputlength)
                    inputlength=b64size;
                  outputlength=32768;
                  if (base64_decode(b64buf,inputlength,payload,&outputlength)!=0) {
                    printf("Failed to decode it.\n");
                  }
                }
              }
              free(b64buf);
            } else {
              printf("couldn't allocate memory for base-64 stuff\n");
            }
            rc = fscanf(stdin,"%64s",datatagend);
            if (strcmp(datatagend,"</data></item>")!=0)
              printf("End data tag not seen, \"%s\" seen instead.\n",datatagend);
            // now, there will either be a line feed or nothing at the end of this line
            // it's not necessary XML, but it's what Shairport Sync puts out, and it makes life a bit easier
            // So, if there's something there and it's not just a linefeed, it's an error
            if ((fgets (str, 1024, stdin)!=NULL) && ((strlen(str)!=1) || (str[0]!=0x0A)))
              printf("Error -- unexpected characters at the end of a base64 section.\n");
          }
        }

        // printf("Got it decoded. Length of decoded string is %u bytes.\n",outputlength);
        payload[outputlength]=0;

        // this has more information about tags, which might be relevant:
        // https://code.google.com/p/ytrack/wiki/DMAP
        switch (code) {
          case 'mper':
            {

              // the following is just diagnostic
              // {
              // printf("'mper' payload is %d bytes long: ", length);
              // char* p = payload;
              // int c;
              // for (c=0; c < length; c++) {
              //   printf("%02x", *p);
              //   p++;
              // }
              // printf("\n");
              // }

            // get the 64-bit number as a uint64_t by reading two uint32_t s and combining them
            uint64_t vl = ntohl(*(uint32_t*)payload); // get the high order 32 bits
            vl = vl << 32; // shift them into the correct location
            uint64_t ul = ntohl(*(uint32_t*)(payload+sizeof(uint32_t))); // and the low order 32 bits
            vl = vl + ul;
            printf("Persistent ID: \"%" PRIx64 "\".\n",vl);

            }
            break;
          case 'asul':
            printf("URL: \"%s\".\n",payload);
            break;
          case 'asal':
            printf("Album Name: \"%s\".\n",payload);
            break;
          case 'asar':
            printf("Artist: \"%s\".\n",payload);
            break;
          case 'ascm':
            printf("Comment: \"%s\".\n",payload);
            break;
          case 'asgn':
            printf("Genre: \"%s\".\n",payload);
            break;
          case 'minm':
            printf("Title: \"%s\".\n",payload);
            break;
          case 'ascp':
            printf("Composer: \"%s\".\n",payload);
            break;
          case 'asdt':
            printf("File kind: \"%s\".\n",payload);
            break;
          case 'assn':
            printf("Sort as: \"%s\".\n",payload);
            break;
          case 'PICT':
            printf("Picture received, length %u bytes.\n",length);
            break;
          case 'clip':
            printf("The AirPlay 2 client at \"%s\" has started a play session.\n",payload);
            break;
          case 'svip':
            printf("The address used by Shairport Sync for this play session is: \"%s\".\n",payload);
            break;
          case 'conn':
            printf("The AirPlay 2 client at \"%s\" is about to select this player. (AirPlay 2 only.)\n",payload);
            break;
          case 'disc':
            printf("The AirPlay 2 client at \"%s\" has released this player. (AirPlay 2 only.)\n",payload);
            break;
          default: if (type=='ssnc') {
              char typestring[5];
              *(uint32_t*)typestring = htonl(type);
              typestring[4]=0;
              char codestring[5];
              *(uint32_t*)codestring = htonl(code);
              codestring[4]=0;
              printf("\"%s\" \"%s\": \"%s\".\n",typestring,codestring,payload);
            }
         }
       } else {
          str[1024]='\0';
          printf("\nXXX Could not decipher: \"%s\".\n",str);
       }
       // flush stdout, to be able to pipe it later
       fflush(stdout);
     }
   }
  return 0;
}
