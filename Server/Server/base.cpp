//#include "base.h"
//
//void body_length(std::size_t new_length) {
//    body_length_ = new_length;
//    if (body_length_ > max_body_length)
//        body_length_ = max_body_length;
//}
//
//bool decode_header() {
//    char header[header_length + 1] = "";
//    std::strncat(header, data_, header_length);
//    body_length_ = std::atoi(header);
//    if (body_length_ > max_body_length) {
//        body_length_ = 0;
//        return false;
//    }
//    return true;
//}
//
//void encode_header() {
//    char header[header_length + 1] = "";
//    std::sprintf(header, "%4d", static_cast<int>(body_length_));
//    std::memcpy(data_, header, header_length);
//}