#include "FactoryResetJson.h"
#include <limits.h>
#include <string.h>

namespace {
class Parser {
 public:
  Parser(const char *p, size_t n) : p_(p), e_(p+n) {}
  void ws(){while(p_<e_&&(*p_==' '||*p_=='\t'||*p_=='\r'||*p_=='\n'))++p_;}
  bool take(char c){ws();if(p_==e_||*p_!=c)return false;++p_;return true;}
  bool done(){ws();return p_==e_;}
  bool str(char *out,size_t cap){ws();if(p_==e_||*p_++!='"'||!cap)return false;size_t n=0;while(p_<e_&&*p_!='"'){char c=*p_++;if(c=='\\'||(unsigned char)c<0x20||n+1>=cap)return false;out[n++]=c;}if(p_==e_||*p_++!='"')return false;out[n]=0;return true;}
  bool uint32(uint32_t &v){ws();if(p_==e_||*p_<'0'||*p_>'9')return false;if(*p_=='0'&&p_+1<e_&&p_[1]>='0'&&p_[1]<='9')return false;uint64_t x=0;while(p_<e_&&*p_>='0'&&*p_<='9'){x=x*10+(*p_++-'0');if(x>UINT32_MAX)return false;}v=(uint32_t)x;return true;}
 private: const char *p_,*e_;
};
}
FactoryResetParseResult FactoryResetJson::parse(const char *json,size_t length,FactoryResetRequest &request){
  request=FactoryResetRequest(); if(!json)return FactoryResetParseResult::MALFORMED; Parser p(json,length); if(!p.take('{'))return FactoryResetParseResult::MALFORMED; unsigned seen=0; bool first=true;
  while(true){p.ws();if(p.take('}'))break;if(!first&&!p.take(','))return FactoryResetParseResult::MALFORMED;first=false;char key[24];if(!p.str(key,sizeof key)||!p.take(':'))return FactoryResetParseResult::MALFORMED;unsigned field=strcmp(key,"expected_revision")==0?1:strcmp(key,"confirm")==0?2:0;if(!field)return FactoryResetParseResult::UNKNOWN_FIELD;if(seen&field)return FactoryResetParseResult::DUPLICATE_FIELD;seen|=field;if(field==1){if(!p.uint32(request.expectedRevision))return FactoryResetParseResult::WRONG_TYPE;}else{char value[20];if(!p.str(value,sizeof value))return FactoryResetParseResult::WRONG_TYPE;if(strcmp(value,"FACTORY RESET")!=0)return FactoryResetParseResult::OUT_OF_RANGE;}}
  if (!p.done()) return FactoryResetParseResult::MALFORMED;
  return seen == 3 ? FactoryResetParseResult::OK
                   : FactoryResetParseResult::MISSING_FIELD;
}
