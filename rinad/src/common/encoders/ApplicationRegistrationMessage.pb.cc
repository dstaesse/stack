// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: ApplicationRegistrationMessage.proto

#define INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION
#include "ApplicationRegistrationMessage.pb.h"

#include <algorithm>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)

namespace rina {
namespace messages {

namespace {

const ::google::protobuf::Descriptor* ApplicationRegistration_descriptor_ = NULL;
const ::google::protobuf::internal::GeneratedMessageReflection*
  ApplicationRegistration_reflection_ = NULL;

}  // namespace


void protobuf_AssignDesc_ApplicationRegistrationMessage_2eproto() {
  protobuf_AddDesc_ApplicationRegistrationMessage_2eproto();
  const ::google::protobuf::FileDescriptor* file =
    ::google::protobuf::DescriptorPool::generated_pool()->FindFileByName(
      "ApplicationRegistrationMessage.proto");
  GOOGLE_CHECK(file != NULL);
  ApplicationRegistration_descriptor_ = file->message_type(0);
  static const int ApplicationRegistration_offsets_[3] = {
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ApplicationRegistration, naminginfo_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ApplicationRegistration, socketnumber_),
    GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ApplicationRegistration, difnames_),
  };
  ApplicationRegistration_reflection_ =
    new ::google::protobuf::internal::GeneratedMessageReflection(
      ApplicationRegistration_descriptor_,
      ApplicationRegistration::default_instance_,
      ApplicationRegistration_offsets_,
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ApplicationRegistration, _has_bits_[0]),
      GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(ApplicationRegistration, _unknown_fields_),
      -1,
      ::google::protobuf::DescriptorPool::generated_pool(),
      ::google::protobuf::MessageFactory::generated_factory(),
      sizeof(ApplicationRegistration));
}

namespace {

GOOGLE_PROTOBUF_DECLARE_ONCE(protobuf_AssignDescriptors_once_);
inline void protobuf_AssignDescriptorsOnce() {
  ::google::protobuf::GoogleOnceInit(&protobuf_AssignDescriptors_once_,
                 &protobuf_AssignDesc_ApplicationRegistrationMessage_2eproto);
}

void protobuf_RegisterTypes(const ::std::string&) {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedMessage(
    ApplicationRegistration_descriptor_, &ApplicationRegistration::default_instance());
}

}  // namespace

void protobuf_ShutdownFile_ApplicationRegistrationMessage_2eproto() {
  delete ApplicationRegistration::default_instance_;
  delete ApplicationRegistration_reflection_;
}

void protobuf_AddDesc_ApplicationRegistrationMessage_2eproto() {
  static bool already_here = false;
  if (already_here) return;
  already_here = true;
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  ::rina::messages::protobuf_AddDesc_ApplicationProcessNamingInfoMessage_2eproto();
  ::google::protobuf::DescriptorPool::InternalAddGeneratedFile(
    "\n$ApplicationRegistrationMessage.proto\022\r"
    "rina.messages\032)ApplicationProcessNamingI"
    "nfoMessage.proto\"\204\001\n\027ApplicationRegistra"
    "tion\022A\n\nnamingInfo\030\001 \001(\0132-.rina.messages"
    ".applicationProcessNamingInfo_t\022\024\n\014socke"
    "tNumber\030\002 \001(\r\022\020\n\010difNames\030\003 \003(\tB;\n9rina."
    "encoding.impl.googleprotobuf.application"
    "registration", 292);
  ::google::protobuf::MessageFactory::InternalRegisterGeneratedFile(
    "ApplicationRegistrationMessage.proto", &protobuf_RegisterTypes);
  ApplicationRegistration::default_instance_ = new ApplicationRegistration();
  ApplicationRegistration::default_instance_->InitAsDefaultInstance();
  ::google::protobuf::internal::OnShutdown(&protobuf_ShutdownFile_ApplicationRegistrationMessage_2eproto);
}

// Force AddDescriptors() to be called at static initialization time.
struct StaticDescriptorInitializer_ApplicationRegistrationMessage_2eproto {
  StaticDescriptorInitializer_ApplicationRegistrationMessage_2eproto() {
    protobuf_AddDesc_ApplicationRegistrationMessage_2eproto();
  }
} static_descriptor_initializer_ApplicationRegistrationMessage_2eproto_;

// ===================================================================

#ifndef _MSC_VER
const int ApplicationRegistration::kNamingInfoFieldNumber;
const int ApplicationRegistration::kSocketNumberFieldNumber;
const int ApplicationRegistration::kDifNamesFieldNumber;
#endif  // !_MSC_VER

ApplicationRegistration::ApplicationRegistration()
  : ::google::protobuf::Message() {
  SharedCtor();
}

void ApplicationRegistration::InitAsDefaultInstance() {
  naminginfo_ = const_cast< ::rina::messages::applicationProcessNamingInfo_t*>(&::rina::messages::applicationProcessNamingInfo_t::default_instance());
}

ApplicationRegistration::ApplicationRegistration(const ApplicationRegistration& from)
  : ::google::protobuf::Message() {
  SharedCtor();
  MergeFrom(from);
}

void ApplicationRegistration::SharedCtor() {
  _cached_size_ = 0;
  naminginfo_ = NULL;
  socketnumber_ = 0u;
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
}

ApplicationRegistration::~ApplicationRegistration() {
  SharedDtor();
}

void ApplicationRegistration::SharedDtor() {
  if (this != default_instance_) {
    delete naminginfo_;
  }
}

void ApplicationRegistration::SetCachedSize(int size) const {
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const ::google::protobuf::Descriptor* ApplicationRegistration::descriptor() {
  protobuf_AssignDescriptorsOnce();
  return ApplicationRegistration_descriptor_;
}

const ApplicationRegistration& ApplicationRegistration::default_instance() {
  if (default_instance_ == NULL) protobuf_AddDesc_ApplicationRegistrationMessage_2eproto();
  return *default_instance_;
}

ApplicationRegistration* ApplicationRegistration::default_instance_ = NULL;

ApplicationRegistration* ApplicationRegistration::New() const {
  return new ApplicationRegistration;
}

void ApplicationRegistration::Clear() {
  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (has_naminginfo()) {
      if (naminginfo_ != NULL) naminginfo_->::rina::messages::applicationProcessNamingInfo_t::Clear();
    }
    socketnumber_ = 0u;
  }
  difnames_.Clear();
  ::memset(_has_bits_, 0, sizeof(_has_bits_));
  mutable_unknown_fields()->Clear();
}

bool ApplicationRegistration::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream* input) {
#define DO_(EXPRESSION) if (!(EXPRESSION)) return false
  ::google::protobuf::uint32 tag;
  while ((tag = input->ReadTag()) != 0) {
    switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
      // optional .rina.messages.applicationProcessNamingInfo_t namingInfo = 1;
      case 1: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
          DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
               input, mutable_naminginfo()));
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(16)) goto parse_socketNumber;
        break;
      }

      // optional uint32 socketNumber = 2;
      case 2: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_VARINT) {
         parse_socketNumber:
          DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                   ::google::protobuf::uint32, ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>(
                 input, &socketnumber_)));
          set_has_socketnumber();
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(26)) goto parse_difNames;
        break;
      }

      // repeated string difNames = 3;
      case 3: {
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED) {
         parse_difNames:
          DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                input, this->add_difnames()));
          ::google::protobuf::internal::WireFormat::VerifyUTF8String(
            this->difnames(this->difnames_size() - 1).data(),
            this->difnames(this->difnames_size() - 1).length(),
            ::google::protobuf::internal::WireFormat::PARSE);
        } else {
          goto handle_uninterpreted;
        }
        if (input->ExpectTag(26)) goto parse_difNames;
        if (input->ExpectAtEnd()) return true;
        break;
      }

      default: {
      handle_uninterpreted:
        if (::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
            ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
          return true;
        }
        DO_(::google::protobuf::internal::WireFormat::SkipField(
              input, tag, mutable_unknown_fields()));
        break;
      }
    }
  }
  return true;
#undef DO_
}

void ApplicationRegistration::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream* output) const {
  // optional .rina.messages.applicationProcessNamingInfo_t namingInfo = 1;
  if (has_naminginfo()) {
    ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
      1, this->naminginfo(), output);
  }

  // optional uint32 socketNumber = 2;
  if (has_socketnumber()) {
    ::google::protobuf::internal::WireFormatLite::WriteUInt32(2, this->socketnumber(), output);
  }

  // repeated string difNames = 3;
  for (int i = 0; i < this->difnames_size(); i++) {
  ::google::protobuf::internal::WireFormat::VerifyUTF8String(
    this->difnames(i).data(), this->difnames(i).length(),
    ::google::protobuf::internal::WireFormat::SERIALIZE);
    ::google::protobuf::internal::WireFormatLite::WriteString(
      3, this->difnames(i), output);
  }

  if (!unknown_fields().empty()) {
    ::google::protobuf::internal::WireFormat::SerializeUnknownFields(
        unknown_fields(), output);
  }
}

::google::protobuf::uint8* ApplicationRegistration::SerializeWithCachedSizesToArray(
    ::google::protobuf::uint8* target) const {
  // optional .rina.messages.applicationProcessNamingInfo_t namingInfo = 1;
  if (has_naminginfo()) {
    target = ::google::protobuf::internal::WireFormatLite::
      WriteMessageNoVirtualToArray(
        1, this->naminginfo(), target);
  }

  // optional uint32 socketNumber = 2;
  if (has_socketnumber()) {
    target = ::google::protobuf::internal::WireFormatLite::WriteUInt32ToArray(2, this->socketnumber(), target);
  }

  // repeated string difNames = 3;
  for (int i = 0; i < this->difnames_size(); i++) {
    ::google::protobuf::internal::WireFormat::VerifyUTF8String(
      this->difnames(i).data(), this->difnames(i).length(),
      ::google::protobuf::internal::WireFormat::SERIALIZE);
    target = ::google::protobuf::internal::WireFormatLite::
      WriteStringToArray(3, this->difnames(i), target);
  }

  if (!unknown_fields().empty()) {
    target = ::google::protobuf::internal::WireFormat::SerializeUnknownFieldsToArray(
        unknown_fields(), target);
  }
  return target;
}

int ApplicationRegistration::ByteSize() const {
  int total_size = 0;

  if (_has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    // optional .rina.messages.applicationProcessNamingInfo_t namingInfo = 1;
    if (has_naminginfo()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(
          this->naminginfo());
    }

    // optional uint32 socketNumber = 2;
    if (has_socketnumber()) {
      total_size += 1 +
        ::google::protobuf::internal::WireFormatLite::UInt32Size(
          this->socketnumber());
    }

  }
  // repeated string difNames = 3;
  total_size += 1 * this->difnames_size();
  for (int i = 0; i < this->difnames_size(); i++) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
      this->difnames(i));
  }

  if (!unknown_fields().empty()) {
    total_size +=
      ::google::protobuf::internal::WireFormat::ComputeUnknownFieldsSize(
        unknown_fields());
  }
  GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
  _cached_size_ = total_size;
  GOOGLE_SAFE_CONCURRENT_WRITES_END();
  return total_size;
}

void ApplicationRegistration::MergeFrom(const ::google::protobuf::Message& from) {
  GOOGLE_CHECK_NE(&from, this);
  const ApplicationRegistration* source =
    ::google::protobuf::internal::dynamic_cast_if_available<const ApplicationRegistration*>(
      &from);
  if (source == NULL) {
    ::google::protobuf::internal::ReflectionOps::Merge(from, this);
  } else {
    MergeFrom(*source);
  }
}

void ApplicationRegistration::MergeFrom(const ApplicationRegistration& from) {
  GOOGLE_CHECK_NE(&from, this);
  difnames_.MergeFrom(from.difnames_);
  if (from._has_bits_[0 / 32] & (0xffu << (0 % 32))) {
    if (from.has_naminginfo()) {
      mutable_naminginfo()->::rina::messages::applicationProcessNamingInfo_t::MergeFrom(from.naminginfo());
    }
    if (from.has_socketnumber()) {
      set_socketnumber(from.socketnumber());
    }
  }
  mutable_unknown_fields()->MergeFrom(from.unknown_fields());
}

void ApplicationRegistration::CopyFrom(const ::google::protobuf::Message& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

void ApplicationRegistration::CopyFrom(const ApplicationRegistration& from) {
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

bool ApplicationRegistration::IsInitialized() const {

  if (has_naminginfo()) {
    if (!this->naminginfo().IsInitialized()) return false;
  }
  return true;
}

void ApplicationRegistration::Swap(ApplicationRegistration* other) {
  if (other != this) {
    std::swap(naminginfo_, other->naminginfo_);
    std::swap(socketnumber_, other->socketnumber_);
    difnames_.Swap(&other->difnames_);
    std::swap(_has_bits_[0], other->_has_bits_[0]);
    _unknown_fields_.Swap(&other->_unknown_fields_);
    std::swap(_cached_size_, other->_cached_size_);
  }
}

::google::protobuf::Metadata ApplicationRegistration::GetMetadata() const {
  protobuf_AssignDescriptorsOnce();
  ::google::protobuf::Metadata metadata;
  metadata.descriptor = ApplicationRegistration_descriptor_;
  metadata.reflection = ApplicationRegistration_reflection_;
  return metadata;
}


// @@protoc_insertion_point(namespace_scope)

}  // namespace messages
}  // namespace rina

// @@protoc_insertion_point(global_scope)
