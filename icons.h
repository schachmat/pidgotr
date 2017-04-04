/* The GOTR pidgin plugin
 * Copyright (C) 2017 Markus Teich
 *
 * This file is part of the GOTR pidgin plugin.
 *
 * The GOTR pidgin plugin is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * The GOTR pidgin plugin is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * the GOTR pidgin plugin.  If not, see <http://www.gnu.org/licenses/>.
 */

static const char green_png[] =
"\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
"\x00\x00\x00\x10\x00\x00\x00\x10\x08\x06\x00\x00\x00\x1f\xf3\xff"
"\x61\x00\x00\x00\x06\x62\x4b\x47\x44\x00\x00\x00\x00\x00\x00\xf9"
"\x43\xbb\x7f\x00\x00\x00\x09\x70\x48\x59\x73\x00\x00\x0b\x13\x00"
"\x00\x0b\x13\x01\x00\x9a\x9c\x18\x00\x00\x00\x07\x74\x49\x4d\x45"
"\x07\xe1\x03\x0e\x0d\x25\x34\x77\x2c\xd8\x65\x00\x00\x00\x1d\x69"
"\x54\x58\x74\x43\x6f\x6d\x6d\x65\x6e\x74\x00\x00\x00\x00\x00\x43"
"\x72\x65\x61\x74\x65\x64\x20\x77\x69\x74\x68\x20\x47\x49\x4d\x50"
"\x64\x2e\x65\x07\x00\x00\x00\xcf\x49\x44\x41\x54\x38\xcb\xd5\x92"
"\x3f\x0e\x01\x41\x14\xc6\x7f\x8b\x0b\xac\x6c\xf4\x2a\x17\x20\x12"
"\xda\xcd\x1e\x44\xaf\x7b\xc7\xf8\x5c\x41\xb5\xa7\x10\xb5\x86\x03"
"\x50\xe9\x65\x23\x2e\x20\x14\xde\xc4\x06\x1b\x7f\xb6\x32\xcd\x64"
"\xde\xbc\xef\xcf\xfb\x66\xa0\xe6\x8a\x1e\x0b\x66\xd6\x04\x32\x60"
"\x02\xa4\x5e\x5e\x02\x73\x60\x21\xe9\x5c\x49\x60\x66\x3d\x60\x05"
"\xb4\x2b\x04\x8f\xc0\x48\xd2\x2e\x14\x1a\x0f\xe0\xad\x83\xa7\x40"
"\x2c\x29\x92\x14\x01\xb1\xd7\xda\xc0\xd6\x7b\xef\x0e\xdc\xf6\xc1"
"\x1b\x06\x92\x36\xaf\xe4\xcd\xac\x0f\xac\xdd\x49\x47\xd2\x39\x38"
"\xc8\x82\x72\x15\x18\xc0\xef\x82\x93\xac\x3c\xc2\xc4\xf7\xfc\x83"
"\xe0\xf3\x32\x26\x10\xa4\xae\x70\x7a\x87\x2e\xf5\xa4\x00\xad\x5f"
"\xde\xde\x83\xbd\x85\x38\x9c\x8d\x2f\xe1\x90\x74\x13\x8a\x7d\xf1"
"\x15\x59\xa3\xee\x4f\xac\x4d\xf0\x94\x41\xd2\x4d\xde\x82\xca\x63"
"\xb6\xaa\x2e\xfe\x27\x83\x2b\x6b\x92\x3b\xc1\xcd\x52\x74\x9b\x00"
"\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60\x82";

static const char yellow_png[] =
"\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
"\x00\x00\x00\x10\x00\x00\x00\x10\x08\x06\x00\x00\x00\x1f\xf3\xff"
"\x61\x00\x00\x00\x06\x62\x4b\x47\x44\x00\xff\x00\xff\x00\xff\xa0"
"\xbd\xa7\x93\x00\x00\x00\x09\x70\x48\x59\x73\x00\x00\x0b\x13\x00"
"\x00\x0b\x13\x01\x00\x9a\x9c\x18\x00\x00\x00\x07\x74\x49\x4d\x45"
"\x07\xe1\x04\x04\x08\x05\x31\x49\x94\xf6\xd7\x00\x00\x00\x1d\x69"
"\x54\x58\x74\x43\x6f\x6d\x6d\x65\x6e\x74\x00\x00\x00\x00\x00\x43"
"\x72\x65\x61\x74\x65\x64\x20\x77\x69\x74\x68\x20\x47\x49\x4d\x50"
"\x64\x2e\x65\x07\x00\x00\x00\xe2\x49\x44\x41\x54\x38\xcb\xad\x52"
"\x2b\x12\xc2\x30\x10\x7d\xe9\x74\xa6\x48\x2e\x10\x43\x6f\x50\xdf"
"\xc1\x20\x11\xd5\xa8\x14\xc7\x09\x90\xcc\x20\x2b\x31\x75\x80\xe2"
"\x16\x88\x4e\x5d\x8e\xd0\x82\xe3\x0e\x19\xd4\xa2\x1a\xda\x24\x65"
"\x0a\x65\x55\xb2\x9b\xbc\xcf\xee\x32\x22\x22\x8c\x08\x1f\x00\xa8"
"\x08\x74\x82\x62\x85\xaa\xaa\x20\xa5\x44\x5d\xd7\x00\x80\xfd\x22"
"\xd3\x75\x36\x7f\x76\x00\x18\x11\x51\x1b\x60\x77\xdd\x42\x29\xa5"
"\xef\xd9\xf2\x60\xb1\xb6\x41\x7c\xb3\xa8\x94\x42\x92\x24\x88\xa2"
"\x08\x13\x39\x75\xca\xa6\x22\xd0\x20\x96\x82\xc7\xec\x06\xce\x79"
"\xc7\x56\xc3\xea\xca\x79\x26\x3a\xe7\xdc\x92\xa9\xd9\x0c\xff\x4e"
"\x0b\x7d\x5e\xcd\x66\x37\xe1\x0d\x1d\x97\x4b\xfe\x60\x80\xbe\xcf"
"\x00\xc0\x84\x10\x74\x14\x17\x9d\x58\x9f\x57\x16\xc0\xa7\xba\x87"
"\x91\xc1\x84\x10\x7a\x95\xe3\x38\x46\x59\x96\xdf\xaf\xf2\x5b\xe2"
"\x05\x40\x6e\x3d\x4a\xc3\x8d\x3e\x9f\xee\x79\x87\xc4\x1a\xa3\x4b"
"\x41\x1a\xf6\xd7\xff\xdb\x83\x5f\xe2\x05\xee\x8a\x5f\x05\xf6\x3f"
"\x08\xbf\x00\x00\x00\x00\x49\x45\x4e\x44\xae\x42\x60\x82";

static const char red_png[] =
"\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
"\x00\x00\x00\x10\x00\x00\x00\x10\x08\x06\x00\x00\x00\x1f\xf3\xff"
"\x61\x00\x00\x00\x06\x62\x4b\x47\x44\x00\xff\x00\xff\x00\xff\xa0"
"\xbd\xa7\x93\x00\x00\x00\x09\x70\x48\x59\x73\x00\x00\x0b\x13\x00"
"\x00\x0b\x13\x01\x00\x9a\x9c\x18\x00\x00\x00\x07\x74\x49\x4d\x45"
"\x07\xe1\x04\x04\x07\x39\x18\x73\x89\x50\x79\x00\x00\x00\x1d\x69"
"\x54\x58\x74\x43\x6f\x6d\x6d\x65\x6e\x74\x00\x00\x00\x00\x00\x43"
"\x72\x65\x61\x74\x65\x64\x20\x77\x69\x74\x68\x20\x47\x49\x4d\x50"
"\x64\x2e\x65\x07\x00\x00\x01\x2d\x49\x44\x41\x54\x38\xcb\xa5\x92"
"\xa1\x6e\xc3\x30\x10\x86\xbf\x58\x25\x91\xfa\x0c\x65\x09\x29\xa8"
"\x64\x10\x12\x15\x8d\xe4\x09\x46\x46\x02\x47\x0a\x4a\x06\x0b\x0a"
"\x13\x50\x30\xb2\x17\x18\xd9\x13\x8c\x14\x45\x23\x06\x91\x0a\x46"
"\x1c\xb6\x67\x98\x64\x66\x0f\x4c\x71\x9d\x2c\x6a\xa5\xee\x90\x73"
"\xbe\xef\xbf\xff\xce\x89\x9c\x73\x8e\x7f\xc4\x0c\xe0\xfb\x2e\x03"
"\x60\x7e\x54\x58\x6b\xd1\x5a\xa3\x94\xa2\xeb\x3a\x00\x92\x24\x21"
"\xcb\x32\xd2\x34\x45\x08\x31\xa8\x8f\x9c\x73\xae\x4f\x00\x54\x79"
"\x81\x31\x66\xb2\x5b\x1c\xc7\x3c\x7d\xbc\xfb\x6f\x2f\x10\xba\x00"
"\x38\xed\x0f\x48\x29\x89\xe3\x18\x00\x63\x0c\x6d\xdb\xb2\xda\x6d"
"\x07\x30\x80\x00\xb0\xd6\x52\xe5\x85\xbf\x5c\xed\xb6\x1e\xee\x3b"
"\x87\x70\x95\x17\x58\x6b\xcf\x02\x5a\x6b\x8c\x31\x9c\xf6\x07\x5f"
"\x14\x3a\x1a\xbb\x33\xc6\xa0\xb5\x3e\x0b\x28\xf5\x6b\x47\x4a\xe9"
"\xad\xf5\x60\x08\xcf\x8f\x0a\x29\x25\x21\x23\x00\xbf\xed\xde\x76"
"\x28\x32\x9e\xb9\xaf\xe9\x99\xd9\x2d\x6f\x5f\xd7\xb5\x3f\x47\x65"
"\x59\xfa\x1f\x69\xbd\x5e\xd3\x34\x0d\xcf\x5f\x9f\x93\xe0\x66\xb1"
"\xfc\x93\x13\xe3\x44\x08\x6f\x16\xcb\x01\x34\x25\x3c\x10\xb8\x7f"
"\x7d\x99\xec\x76\x49\xc4\x8f\x10\x5e\xbc\x3d\x3c\x4e\x8e\x30\xd5"
"\x20\x2a\xcb\xd2\x8d\x6d\x5f\x8a\x71\xad\xb8\xb6\xa4\x6b\x8b\x1c"
"\xbc\xc2\x2d\xf1\x03\xdd\xfa\x8c\x4e\x0c\x3d\x0a\x3e\x00\x00\x00"
"\x00\x49\x45\x4e\x44\xae\x42\x60\x82";
