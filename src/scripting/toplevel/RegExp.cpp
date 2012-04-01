/**************************************************************************
    Lightspark, a free flash player implementation

    Copyright (C) 2009-2011  Alessandro Pignotti (a.pignotti@sssup.it)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**************************************************************************/

#include <pcre.h>
#include "argconv.h"
#include "RegExp.h"

using namespace std;
using namespace lightspark;

SET_NAMESPACE("");
REGISTER_CLASS_NAME(RegExp);

RegExp::RegExp():dotall(false),global(false),ignoreCase(false),extended(false),multiline(false),lastIndex(0)
{
}

RegExp::RegExp(const tiny_string& _re):dotall(false),global(false),ignoreCase(false),extended(false),multiline(false),lastIndex(0),source(_re)
{
}

void RegExp::sinit(Class_base* c)
{
	c->setSuper(Class<ASObject>::getRef());
	c->setConstructor(Class<IFunction>::getFunction(_constructor));
	c->setDeclaredMethodByQName("exec",AS3,Class<IFunction>::getFunction(exec),NORMAL_METHOD,true);
	c->setDeclaredMethodByQName("test",AS3,Class<IFunction>::getFunction(test),NORMAL_METHOD,true);
	c->prototype->setVariableByQName("toString",AS3,Class<IFunction>::getFunction(_toString),DYNAMIC_TRAIT);
	REGISTER_GETTER(c,dotall);
	REGISTER_GETTER(c,global);
	REGISTER_GETTER(c,ignoreCase);
	REGISTER_GETTER(c,extended);
	REGISTER_GETTER(c,multiline);
	REGISTER_GETTER_SETTER(c,lastIndex);
	REGISTER_GETTER(c,source);
}

void RegExp::buildTraits(ASObject* o)
{
}

ASFUNCTIONBODY(RegExp,_constructor)
{
	RegExp* th=static_cast<RegExp*>(obj);
	if(argslen > 0 && args[0]->is<RegExp>())
	{
		if(argslen > 1 && !args[1]->is<Undefined>())
			throw Class<TypeError>::getInstanceS("flags must be Undefined");
		RegExp *src=args[0]->as<RegExp>();
		th->source=src->source;
		th->dotall=src->dotall;
		th->global=src->global;
		th->ignoreCase=src->ignoreCase;
		th->extended=src->extended;
		th->multiline=src->multiline;
		return NULL;
	}
	else if(argslen > 0)
		th->source=args[0]->toString().raw_buf();
	if(argslen>1 && !args[1]->is<Undefined>())
	{
		const tiny_string& flags=args[1]->toString();
		for(auto i=flags.begin();i!=flags.end();++i)
		{
			switch(*i)
			{
				case 'g':
					th->global=true;
					break;
				case 'i':
					th->ignoreCase=true;
					break;
				case 'x':
					th->extended=true;
					break;
				case 'm':
					th->multiline=true;
					break;
				case 's':
					// Defined in the Adobe online
					// help but not in ECMA
					th->dotall=true;
					break;
				default:
					throw Class<SyntaxError>::getInstanceS("unknown flag in RegExp");
			}
		}
	}
	return NULL;
}


ASFUNCTIONBODY(RegExp,generator)
{
	if(args[0]->is<RegExp>())
	{
		args[0]->incRef();
		return args[0];
	}
	else
	{
		if (argslen > 1)
			LOG(LOG_NOT_IMPLEMENTED, "RegExp generator: flags argument not implemented");
		return Class<RegExp>::getInstanceS(args[0]->toString());
	}
}

ASFUNCTIONBODY_GETTER(RegExp, dotall);
ASFUNCTIONBODY_GETTER(RegExp, global);
ASFUNCTIONBODY_GETTER(RegExp, ignoreCase);
ASFUNCTIONBODY_GETTER(RegExp, extended);
ASFUNCTIONBODY_GETTER(RegExp, multiline);
ASFUNCTIONBODY_GETTER_SETTER(RegExp, lastIndex);
ASFUNCTIONBODY_GETTER(RegExp, source);

ASFUNCTIONBODY(RegExp,exec)
{
	RegExp* th=static_cast<RegExp*>(obj);
	assert_and_throw(argslen==1);
	const tiny_string& arg0=args[0]->toString();
	return th->match(arg0);
}

ASObject *RegExp::match(const tiny_string& str)
{
	const char* error;
	int errorOffset;
	int options=PCRE_UTF8;
	if(ignoreCase)
		options|=PCRE_CASELESS;
	if(extended)
		options|=PCRE_EXTENDED;
	if(multiline)
		options|=PCRE_MULTILINE;
	if(dotall)
		options|=PCRE_DOTALL;
	pcre* pcreRE=pcre_compile(source.raw_buf(), options, &error, &errorOffset,NULL);
	if(error)
		return new Null;
	//Verify that 30 for ovector is ok, it must be at least (captGroups+1)*3
	int capturingGroups;
	int infoOk=pcre_fullinfo(pcreRE, NULL, PCRE_INFO_CAPTURECOUNT, &capturingGroups);
	if(infoOk!=0)
	{
		pcre_free(pcreRE);
		return new Null;
	}
	assert_and_throw(capturingGroups<10);
	//Get information about named capturing groups
	int namedGroups;
	infoOk=pcre_fullinfo(pcreRE, NULL, PCRE_INFO_NAMECOUNT, &namedGroups);
	if(infoOk!=0)
	{
		pcre_free(pcreRE);
		return new Null;
	}
	//Get information about the size of named entries
	int namedSize;
	infoOk=pcre_fullinfo(pcreRE, NULL, PCRE_INFO_NAMEENTRYSIZE, &namedSize);
	if(infoOk!=0)
	{
		pcre_free(pcreRE);
		return new Null;
	}
	struct nameEntry
	{
		uint16_t number;
		char name[0];
	};
	char* entries;
	infoOk=pcre_fullinfo(pcreRE, NULL, PCRE_INFO_NAMETABLE, &entries);
	if(infoOk!=0)
	{
		pcre_free(pcreRE);
		return new Null;
	}

	int ovector[30];
	int offset=global?lastIndex:0;
	int rc=pcre_exec(pcreRE, NULL, str.raw_buf(), str.numBytes(), offset, 0, ovector, 30);
	if(rc<0)
	{
		//No matches or error
		pcre_free(pcreRE);
		return new Null;
	}
	Array* a=Class<Array>::getInstanceS();
	//Push the whole result and the captured strings
	for(int i=0;i<capturingGroups+1;i++)
	{
		if(ovector[i*2] != -1)
			a->push(Class<ASString>::getInstanceS( str.substr_bytes(ovector[i*2],ovector[i*2+1]-ovector[i*2]) ));
		else
			a->push(new Undefined);
	}
	a->setVariableByQName("input","",Class<ASString>::getInstanceS(str),DYNAMIC_TRAIT);

	// pcre_exec returns byte position, so we have to convert it to character position 
	tiny_string tmp = str.substr_bytes(0, ovector[0]);
	int index = tmp.numChars();

	a->setVariableByQName("index","",abstract_i(index),DYNAMIC_TRAIT);
	for(int i=0;i<namedGroups;i++)
	{
		nameEntry* entry=(nameEntry*)entries;
		uint16_t num=GINT16_FROM_BE(entry->number);
		ASObject* captured=a->at(num);
		captured->incRef();
		a->setVariableByQName(tiny_string(entry->name, true),"",captured,DYNAMIC_TRAIT);
		entries+=namedSize;
	}
	lastIndex=ovector[1];
	pcre_free(pcreRE);
	return a;
}

ASFUNCTIONBODY(RegExp,test)
{
	RegExp* th=static_cast<RegExp*>(obj);

	const tiny_string& arg0 = args[0]->toString();

	int options = PCRE_UTF8;
	if(th->ignoreCase)
		options |= PCRE_CASELESS;
	if(th->extended)
		options |= PCRE_EXTENDED;
	if(th->multiline)
		options |= PCRE_MULTILINE;
	if(th->dotall)
		options|=PCRE_DOTALL;

	const char * error;
	int errorOffset;
	pcre * pcreRE = pcre_compile(th->source.raw_buf(), options, &error, &errorOffset, NULL);
	if(error)
		return new Null;

	int ovector[30];
	int offset=(th->global)?th->lastIndex:0;
	int rc = pcre_exec(pcreRE, NULL, arg0.raw_buf(), arg0.numBytes(), offset, 0, ovector, 30);
	bool ret = (rc >= 0);
	pcre_free(pcreRE);

	return abstract_b(ret);
}

ASFUNCTIONBODY(RegExp,_toString)
{
	if(!obj->is<RegExp>())
		throw Class<TypeError>::getInstanceS("RegExp.toString is not generic");

	RegExp* th=static_cast<RegExp*>(obj);
	tiny_string ret;
	ret = "/";
	ret += th->source;
	ret += "/";
	if(th->global)
		ret += "g";
	if(th->ignoreCase)
		ret += "i";
	if(th->multiline)
		ret += "m";
	if(th->dotall)
		ret += "s";
	return Class<ASString>::getInstanceS(ret);
}