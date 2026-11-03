#ifndef LNJ_EXACT_PSI_READ_ONLY_TYPES_H
#define LNJ_EXACT_PSI_READ_ONLY_TYPES_H
#include <typeinfo>
#include <type_traits>
namespace lnjCandidate {
// Caller supplies only source-audited concrete types, not an interface base.
// No isA/dynamic_cast admission of unknown derived implementations.
template<class... Types>struct ExactPsiTypePack;
template<>struct ExactPsiTypePack<> {
 template<class Interface>static bool test(const Interface&){return false;}
};
template<class First,class... Rest>struct ExactPsiTypePack<First,Rest...> {
 template<class Interface>static bool test(const Interface& interface){
  return typeid(interface)==typeid(First)||ExactPsiTypePack<Rest...>::test(interface);
 }
};
template<class... AuditedExactTypes,class Interface>
bool exactPsiReadOnlyType(const Interface& interface){
 static_assert(std::is_polymorphic<Interface>::value,"dynamic RTTI required");
 return ExactPsiTypePack<AuditedExactTypes...>::test(interface);
}
}
#endif
