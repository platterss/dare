#ifndef LINKS_H
#define LINKS_H

#include <string_view>

namespace Link {
namespace Auth {
inline constexpr std::string_view AUTH_AJAX = "https://reg.oci.fhda.edu/StudentRegistrationSsb/login/authAjax";
inline constexpr std::string_view SAML_LOGIN = "https://reg.oci.fhda.edu/StudentRegistrationSsb/saml/login";
inline constexpr std::string_view IDP_SSO = "https://ssoshib.fhda.edu/idp/profile/SAML2/POST/SSO";
inline constexpr std::string_view LOGIN_PAGE = "https://ssoshib.fhda.edu/idp/profile/SAML2/POST/SSO?execution=e1s1";
inline constexpr std::string_view SELF_SERVICE_SSO = "https://reg.oci.fhda.edu/StudentRegistrationSsb/saml/SSO";
} // namespace Auth

namespace Reg {
inline constexpr std::string_view TERM_SELECT_CLASS_REG = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/term/termSelection?mode=registration";
inline constexpr std::string_view TERM_CONFIRM_CLASS_REG = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/term/search?mode=registration";
inline constexpr std::string_view TERM_SELECT_PRE_REG = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/term/termSelection?mode=preReg";
inline constexpr std::string_view TERM_CONFIRM_PRE_REG = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/term/search?mode=preReg";
inline constexpr std::string_view PREPARE_REG = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/prepareRegistration/prepareRegistration";
inline constexpr std::string_view BATCH = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/classRegistration/submitRegistration/batch";
inline constexpr std::string_view ADD_CRN_REG_ITEMS = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/classRegistration/addCRNRegistrationItems";
inline constexpr std::string_view REG_DASHBOARD = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/registration";
inline constexpr std::string_view CLASS_REG = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/classRegistration/classRegistration";
} // namespace Reg

namespace Classes {
inline constexpr std::string_view SECTION_DETAILS = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/classRegistration/getSectionDetailsFromCRN";
inline constexpr std::string_view ENROLLMENT_INFO = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/searchResults/getEnrollmentInfo";
} // namespace Classes

namespace Terms {
inline constexpr std::string_view TERMS = "https://reg.oci.fhda.edu/StudentRegistrationSsb/ssb/classSearch/getTerms";
} // namespace Terms

namespace GitHub {
inline constexpr std::string_view REPO_LATEST_RELEASE = "https://api.github.com/repos/platterss/dare/releases/latest";
} // namespace GitHub

namespace Discord {
inline constexpr std::string_view PROFILE_PICTURE = "https://acso-oh.us/wp-content/uploads/2021/04/DARE-logo.jpeg";
} // namespace Discord
} // namespace Link

#endif // LINKS_H