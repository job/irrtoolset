//  $Id$
// Copyright (c) 2001,2002                        RIPE NCC
//
// All Rights Reserved
//
// Permission to use, copy, modify, and distribute this software and its
// documentation for any purpose and without fee is hereby granted,
// provided that the above copyright notice appear in all copies and that
// both that copyright notice and this permission notice appear in
// supporting documentation, and that the name of the author not be
// used in advertising or publicity pertaining to distribution of the
// software without specific, written prior permission.
//
// THE AUTHOR DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
// ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS; IN NO EVENT SHALL
// AUTHOR BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY
// DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
// AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
// OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//
//
//  Copyright (c) 1994 by the University of Southern California
//  All rights reserved.
//
//    Permission is hereby granted, free of charge, to any person obtaining a copy
//    of this software and associated documentation files (the "Software"), to deal
//    in the Software without restriction, including without limitation the rights
//    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//    copies of the Software, and to permit persons to whom the Software is
//    furnished to do so, subject to the following conditions:
//
//    The above copyright notice and this permission notice shall be included in
//    all copies or substantial portions of the Software.
//
//    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//    THE SOFTWARE.
//
//  Questions concerning this software should be directed to 
//  irrtoolset@cs.usc.edu.
//
//  Author(s): Cengiz Alaettinoglu <cengiz@ISI.EDU>

#include "config.h"
#include <istream>
#include <cstdio>
#include <sstream>
#include <fstream>
#include <iomanip>

#include "object.hh"
#include "schema.hh"

using namespace std;

extern int rpslparse(void);
extern void rpsl_scan_object(Object *);

Object::~Object() {
   attrs.clear();
}

const int MAX_KEY_LENGTH       = 1024;
const int INITIAL_CHUNK_SIZE   = 1024;
const int TOO_SMALL_CHUNK_SIZE = 64;


bool Object::read(istream &in) {
   return read(*this, in);
}

bool Object::read(Buffer &buf, istream &in) {
   if (in.eof())
      return false;

   int size = INITIAL_CHUNK_SIZE;
   int remaining = size;

   char *text = (char *) malloc(size);
   char *start = text;
   char *p;

   int linelen;
   int len = 0;

   while (1) {
      in.getline(start, remaining);

      linelen = strlen(start);
      remaining -= linelen;
      len       += linelen;
      start     += linelen;

      if (!linelen || in.eof()) // empty line or eof => end of object
	 break;
      // blank line => end of object?
      for (p = start - linelen; 
	   *p && (*p == ' ' || *p == '\t' || *p == '\r'); ++p)
	 ;
      if (! *p)
	 break;

      if (remaining < TOO_SMALL_CHUNK_SIZE) {
	 remaining += size;
	 size *= 2;
	 text = (char *) realloc(text, size);
	 start = text + len;
      }
      
      if (in) { // append \n if we successfully read a line
	 *start = '\n';
	 start++;
	 len++;
	 remaining--;
      } else
	 in.clear();
   }

   buf.size=len;
   buf.contents = (char *) realloc(text, len+2);

   return len;
}

void Object::parse() {
   rpsl_scan_object(this);
   rpslparse();
   validate();

   if (type) {
      bool forgiving = schema.isForgiving();
      if (isDeleted || schema.isVeryForgiving()) {
	      if (has_error) {
   	      has_error = false;

   	      for (Attr *attr = attrs.head(); attr; attr = attrs.next(attr))
	          if (! attr->errors.empty() 
  		         && attr->type && attr->type->isKey()) {
         		  has_error = true;
	        	  break;
	          }
       	 }
	       schema.beForgiving();
      }
      has_error |= ! type->validate(errors);
      schema.beForgiving(forgiving);
   } else {
      errors = "***Error: Unknown class encountered.";
      has_error = true;
   }
}

bool Object::scan(ostream &err) {
   parse();

   if (has_error)
      reportErrors(err);

   return ! has_error;
}

bool Object::scan(const char *_text, const int sz, ostream &err) {
   contents = (char *) _text;
   size     = sz;

   scan(err);

   contents = NULL;
   size     = 0;
   return ! has_error;
}

void Object::reportErrors(ostream &ostrm) {
   Attr *n_attr;

   for (Attr *attr = attrs.head(); attr; attr = n_attr) {
      n_attr = attrs.next(attr);
      if (attr->errors.empty() || attr->errorLeng == 0) {
	 ostrm.write(&contents[attr->offset], attr->len);
	 ostrm << attr->errors;
      } else {
	 char *begin = &contents[attr->offset];
	 char *end = begin;
	 for (int i = 0; i <= attr->errorLine; ++i)
	    end = strchr(end, '\n') + 1;
	 ostrm.write(begin, end - begin);
	 for (int i = 0; i < attr->errorColon; ++i)
	    ostrm << " ";
	 for (int i = 0; i < attr->errorLeng; ++i)
	    ostrm << "^";
	 ostrm << "\n";
	 ostrm << attr->errors;
       	 ostrm.write(end, attr->len - (end - begin));
      }
   }
   if (! attrs.head() && contents)
      ostrm.write(contents, size);

   ostrm << errors;
   ostrm << "\n";
}

#ifdef ENABLE_DEBUG

#define PRINT1(x) os << "  " #x " = " << x << endl
#define PRINT2(x, y) os << "  " #x " = " << y << endl

void Object::printPTree(ostream &os) const
{
  os << "Object" << endl; 
  PRINT2(contents, "\"...\"");
  PRINT1(size);
  os << "  type" << endl;
  os << "  "; PRINT2(name, type->name);
  os << "  attrs (List <Attr>)" << endl;
  os << "  "; PRINT2(length, attrs.size());
  for (Attr *attr = attrs.head(); attr; attr = attrs.next(attr)) {
    os << "    ListNode" << endl;
    //    os << "      forw" << endl;
    //    os << "      back" << endl;
    os << "      data (Attr *)" << endl;
    os << "      "; PRINT2(offset, attr->offset);
    os << "      "; PRINT2(len, attr->len);
    os << "      " << attr->className() << endl; 
    attr->printClass(os, 10);
  }
}

#endif // #ifdef ENABLE_DEBUG

ostream& operator<<(ostream &os, const Object &o) {
   Attr *attr;
      
   for (attr = o.attrs.head(); attr; attr = o.attrs.next(attr))
      if (attr->type && !attr->type->isObsolete())
	 os << *attr << "\n";

   os << "\n";
   return os;
}

bool Object::setClass(char *cls) {
   type = schema.searchClass(cls);
   // make sure there is an extra \n at the end
   append("\n", 1);
   return type;
}

bool Object::hasAttr(const char *name) {
  Attr *attr;
  for (attr = attrs.head(); attr; attr = attrs.next(attr)) {
     if (strcasecmp(attr->type->name(), name) == 0) 
        return true;
  }
  return false;
}

void Object::validate() {
  if (type && (strcasecmp(type->name, "filter-set") == 0)) {
    Attr *attr;
    static char buffer[1024];
    for (attr = attrs.head(); attr; attr = attrs.next(attr)) {
      if (strcasecmp(attr->type->name(), "filter") == 0 || strcasecmp(attr->type->name(), "mp-filter") == 0)
        return;
    }
    sprintf(buffer, "***Error: either filter or mp-filter must be present.\n");
    
    errors += buffer;
    has_error = true;
  }
  else if (type && (strcasecmp(type->name, "peering-set") == 0)) {
    Attr *attr;
    static char buffer[1024];
    for (attr = attrs.head(); attr; attr = attrs.next(attr)) {
      if (strcasecmp(attr->type->name(), "peering") == 0 || strcasecmp(attr->type->name(), "mp-peering") == 0)
        return;
    }
    sprintf(buffer, "***Error: at least one from peering or mp-peering must be present.\n");

    errors += buffer;
    has_error = true;
  }
}


bool Object::addAttr(char *attr, Item *item) {
   if (!type)
      return false;

   const AttrAttr *attrType = type->searchAttr(attr);
   if (!attrType)
      return false;

   ItemList *items = new ItemList;
   items->append(item);
   Attr *attrib = new AttrGeneric(attrType, items);

   ostringstream os;
   os << *attrib << ends;
   attrib->offset = size;
   attrib->len    = (os.str()).length();

   // delete the extra \n at the end, and reinsert after this attribute
   size--;
   append((os.str()).c_str(), static_cast<unsigned long>(attrib->len));
   append("\n", 1);

   (*this) += attrib;
   return true;
}

bool Object::setAttr(char *attrName, Item *item) {
   if (!type)
      return false;

   const AttrAttr *attrType = type->searchAttr(attrName);
   if (!attrType)
      return false;

   ItemList *items = new ItemList;
   items->append(item);
   AttrGeneric *attr = new AttrGeneric(attrType, items);
  
   return setAttr(attrName, attr);
}

bool Object::setAttr(char *attrName, Attr *attr) {
   if (!type)
      return false;

   const AttrAttr *attrType = type->searchAttr(attrName);
   if (!attrType)
      return false;

   Attr *atr2;
   for (Attr *atr = attrs.head(); atr; atr = atr2) {
      atr2 = attrs.next(atr);
      if (atr->type == attrType) {
	 attrs.remove(atr);
	 flush(atr->len, atr->offset);
	 delete atr;
      }
   }

   (*this) += attr;

   ostringstream os;
   os << *attr << "\n" << ends;
   attr->offset = size;
   attr->len    = (os.str()).length();
   
   // delete the extra \n at the end, and reinsert after this attribute
   size--;
   append((os.str()).c_str(), static_cast<unsigned long>(attr->len));
   append("\n", 1);

   return true;
}

