#include "ProvisioningWifiJson.h"

#include <string.h>

namespace {
class PasswordWiper {
 public:
  PasswordWiper(char *value, size_t length) : value_(value), length_(length) {}
  ~PasswordWiper() {
    volatile char *wipe = value_;
    for (size_t index = 0; index < length_; ++index) wipe[index] = 0;
  }
 private:
  char *value_;
  size_t length_;
};
class Parser {
 public:
  Parser(const char *data, size_t length) : cursor_(data), end_(data + length) {}
  void spaces(){while(cursor_<end_&&(*cursor_==' '||*cursor_=='\t'||*cursor_=='\r'||*cursor_=='\n'))++cursor_;}
  bool take(char c){spaces();if(cursor_>=end_||*cursor_!=c)return false;++cursor_;return true;}
  bool done(){spaces();return cursor_==end_;}
  bool string(char *out,size_t capacity){
    spaces(); if(cursor_>=end_||*cursor_++!='"'||capacity==0)return false;
    size_t length=0;
    while(cursor_<end_&&*cursor_!='"'){
      unsigned char c=static_cast<unsigned char>(*cursor_++);
      if(c<0x20U)return false;
      if(c=='\\'){
        if(cursor_>=end_)return false;
        const char escaped=*cursor_++;
        if(escaped=='"'||escaped=='\\'||escaped=='/')c=escaped;
        else return false;
      }
      if(length+1U>=capacity)return false;
      out[length++]=static_cast<char>(c);
    }
    if(cursor_>=end_||*cursor_++!='"')return false;
    out[length]='\0'; return true;
  }
 private: const char *cursor_; const char *end_;
};
}

ProvisioningWifiParseResult ProvisioningWifiJson::parse(
    const char *json, size_t length, WifiCredentials &credentials) {
  credentials = WifiCredentials();
  if(json==nullptr)return ProvisioningWifiParseResult::MALFORMED;
  Parser parser(json,length);
  if(!parser.take('{'))return ProvisioningWifiParseResult::MALFORMED;
  uint8_t seen=0; bool first=true;
  char ssid[WifiCredentials::SSID_CAPACITY] = {};
  char password[WifiCredentials::PASSWORD_CAPACITY] = {};
  PasswordWiper passwordWiper(password, sizeof(password));
  char security[12] = {};
  while(true){
    parser.spaces(); if(parser.take('}'))break;
    if(!first&&!parser.take(','))return ProvisioningWifiParseResult::MALFORMED;
    first=false; char key[12];
    if(!parser.string(key,sizeof(key))||!parser.take(':'))return ProvisioningWifiParseResult::MALFORMED;
    uint8_t field=strcmp(key,"ssid")==0?1U:strcmp(key,"password")==0?2U:
                  strcmp(key,"security")==0?4U:0U;
    if(field==0)return ProvisioningWifiParseResult::UNKNOWN_FIELD;
    if(seen&field)return ProvisioningWifiParseResult::DUPLICATE_FIELD;
    seen|=field;
    char *value=field==1U?ssid:field==2U?password:security;
    const size_t valueCapacity=field==1U?sizeof(ssid):field==2U?
        sizeof(password):sizeof(security);
    if(!parser.string(value,valueCapacity)) {
      return ProvisioningWifiParseResult::OUT_OF_RANGE;
    }
  }
  ProvisioningWifiParseResult result=ProvisioningWifiParseResult::OK;
  if(!parser.done())result=ProvisioningWifiParseResult::MALFORMED;
  else if((seen&3U)!=3U)result=ProvisioningWifiParseResult::MISSING_FIELD;
  else if(!credentials.setSsid(ssid))result=ProvisioningWifiParseResult::OUT_OF_RANGE;
  else if((seen&4U)!=0U && strcmp(security,"OPEN")==0) {
    if(password[0]!='\0')result=ProvisioningWifiParseResult::OUT_OF_RANGE;
    else credentials.open=true;
  } else if((seen&4U)==0U || strcmp(security,"PROTECTED")==0) {
    if(!credentials.setPassword(password))result=ProvisioningWifiParseResult::OUT_OF_RANGE;
  } else result=ProvisioningWifiParseResult::OUT_OF_RANGE;
  if(result!=ProvisioningWifiParseResult::OK)credentials=WifiCredentials();
  return result;
}
