#include <string>
#include <unordered_set>

#include "ASTString.hh"
#include "ASTStringList.hh"
#include "ASTVerificationVisitor.hh"
#include "ASTTag.hh"
#include "checksieve.h"
#include "Test.hh"

namespace sieve {

Test::Test() {
    _usage_map["allof"]                 = "allof <tests: test-list>";
    _usage_map["anyof"]                 = "anyof <tests: test-list>";
    _usage_map["exists"]                = "exists [:mime] [:anychild] <header-names: string-list>\n";
    _usage_map["environment"]           =        "environment [COMPARATOR] [MATCH-TYPE] <name: string>                    \n"
                                          "                   <key-list: string-list>                                     \n"
                                          "                   Where name is one of: domain, host, location, name, phase,  \n"
                                          "                                         remote-host, remote-ip, or version    \n"
                                          "                                                                               \n"
                                          "                   If using 'imapsieve' capability, name can be one of:        \n"
                                          "                                         imap.user, imap.email, imap.cause,    \n"
                                          "                                         imap.mailbox, imap.changedflags       \n";
    _usage_map["expiration"]            =        "expiration :comparator i;ascii-numeric [MATCH-TYPE]                     \n"
                                          "                   <unit: day / minute / second> <key-list: string-list>       \n";
    _usage_map["hasexpiration"]         = "hasexpiration";
    _usage_map["header"]                =        "header [:mime] [:anychild] [:regex]                                     \n"
                                          "              [:type / :subtype / :contenttype / :param <params: string-list>] \n"
                                          "              [:comparator <string>]                                           \n"
                                          "              [:is / :contains / :matches]                                     \n"
                                          "              <header-names: string-list> <key-list: string-list>              \n";
    _usage_map["ihave"]                 = "ihave <capabilities: string-list>";
    _usage_map["not"]                   = "not <test1: test>";
    _usage_map["size"]                  = "size <:over / :under> <limit: number>";
    _usage_map["valid_notify_method"]   = "valid_notify_method <notification-uris: string-list>";
    _usage_map["valid_ext_list"]        = "valid_ext_list <ext-list-names: string-list>";

    _validation_fn_map["allof"]                 = &Test::_validateHasOnlyTestList;
    _validation_fn_map["anyof"]                 = &Test::_validateHasOnlyTestList;
    _validation_fn_map["environment"]           = &Test::_validateEnvironmentTest;
    _validation_fn_map["exists"]                = &Test::_validateExists;
    _validation_fn_map["hasexpiration"]         = &Test::_validateHasExpirationTest;
    _validation_fn_map["expiration"]            = &Test::_validateExpirationTest;
    _validation_fn_map["header"]                = &Test::_validateHeaderTest;
    _validation_fn_map["ihave"]                 = &Test::_validateIhaveTest;
    _validation_fn_map["not"]                   = &Test::_validateNotTest;
    _validation_fn_map["size"]                  = &Test::_validateSizeTest;
    _validation_fn_map["valid_notify_method"]   = &Test::_validateValidNotifyMethodTest;
    _validation_fn_map["valid_ext_list"]        = &Test::_validateValidExtListTest;
}

bool Test::validate(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    
    if (!_validation_fn_map[test->value()]) {
        DEBUGLOG(test->value() + " test is missing validation.")
        return true;
    }

    return (this->_validation_fn_map[test->value()])(test);
}

std::string Test::usage(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    return "Usage: " + _usage_map[test->value()];
}

//-- Private Members
bool Test::_validateExists(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    size_t size = test->children().size();
    
    if (size < 1 || size > 3)
        return false;
    
    const auto *stringChild = dynamic_cast<const ASTString*>(test->children()[size - 1]);
    const auto *stringListChild = dynamic_cast<const ASTStringList*>(test->children()[size - 1]);
    if (stringChild == nullptr && stringListChild == nullptr)
        return false;
    
    // If we have more than 1 child, check that they are either :mime or :anychild
    if (size > 1) {
        const auto *firstTag = dynamic_cast<const ASTTag*>(test->children()[0]);
        if (firstTag == nullptr)
            return false;
    
        if (firstTag->value() != ":mime" && firstTag->value() != ":anychild")
            return false;
    }
    
    if (size > 2) {
        const auto *secondTag = dynamic_cast<const ASTTag*>(test->children()[1]);
        if (secondTag == nullptr)
            return false;
    
        if (secondTag->value() != ":mime" && secondTag->value() != ":anychild")
            return false;
    }
    
    return true;
}

bool Test::_validateHasOnlyTestList(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    std::vector<sieve::ASTNode *> children = test->children();
    
    for (auto & it : children) {
        const ASTTestList *child = dynamic_cast<ASTTestList*>(it);
    
        if (child == nullptr)
            return false;
    }
    
    return true;
}

bool Test::_validateNotTest(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    size_t size = test->children().size();

    // Child should be a single test, no test-list allowed
    if (!nodeIsType<ASTTest>(test->children()[0]))
        return false;
    
    if (size != 1)
        return false;
    
    return true;
}

bool Test::_validateSizeTest(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    size_t size = test->children().size();
    
    if (size != 2)
        return false;
    
    const auto *tag = dynamic_cast<const ASTTag*>(test->children()[0]);
    if (tag == nullptr || (tag->value() != ":over" && tag->value() != ":under"))
        return false;
    
    return true;
}

bool Test::_validateValidNotifyMethodTest(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    size_t size = test->children().size();
    
    if (size != 1)
        return false;
    
    if (!nodeIsType<ASTStringList>(test->children()[0]) && !nodeIsType<ASTString>(test->children()[0]))
        return false;
    
    return true;
}

bool Test::_validateHasExpirationTest(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest *>(node);
    return test->children().size() == 0;
}

bool Test::_validateExpirationTest(const ASTNode *node) {
    std::vector<ASTNode *> children = node->children();
    size_t size = children.size();

    if (size < 4) return false;

    const auto *tag = dynamic_cast<const ASTTag *>(children[0]);
    const auto *asciinumeric = dynamic_cast<const ASTString *>(children[1]);
    const auto *match = dynamic_cast<const ASTString *>(children[2]);
    const auto *unit = dynamic_cast<const ASTString *>(children[3]);

    if (tag == nullptr || 
        asciinumeric == nullptr || 
        match == nullptr || 
        unit == nullptr) {
            return false;
    }

    if (tag->value() != ":comparator") return false;
    if (asciinumeric->value() != "i;ascii-numeric") return false;

    const std::unordered_set<std::string_view> units = {"day", "minute", "second"};
    if (units.find(unit->value()) == units.end()) return false;

    return true;
}

bool Test::_validateHeaderTest(const ASTNode *node) {
    std::vector<ASTNode *> children = node->children();
    size_t size = children.size();
    
    if (size < 2)
        return false;
    
    for (auto & it : children) {
        const auto *tag = dynamic_cast<const ASTTag*>(it);
    
        if (tag != nullptr) {
            std::string tagValue;
            tagValue = tag->value();
    
            if (tagValue == ":is" ||
                tagValue == ":matches" ||
                tagValue == ":contains" ||
                tagValue == ":comparator" ||
                tagValue == ":mime" ||
                tagValue == ":anychild" ||
                tagValue == ":type" ||
                tagValue == ":subtype" ||
                tagValue == ":contenttype" ||
                tagValue == ":param" ||
                tagValue == ":regex" ||
                tagValue == ":value" ||
                tagValue == ":count" ||
                tagValue == ":index" ||
                tagValue == ":list") {
                    continue;
                }
            
            return false;
        }
    } 
    
    return true;
}

bool Test::_validateIhaveTest(const ASTNode *node) {
    std::vector<ASTNode *> children = node->children();
    size_t size = children.size();
    
    if (size != 1)
        return false;
    
    const auto *string_child = dynamic_cast<const ASTString*>(children[0]);
    const auto *stringlist_child = dynamic_cast<const ASTStringList*>(children[0]);
    
    if (string_child == nullptr && stringlist_child == nullptr)
        return false;
    
    return true;
}

bool Test::_validateEnvironmentTest(const ASTNode *node) {
    std::vector<ASTNode *> children = node->children();
    size_t size = children.size();
    
    if (size < 2)
        return false;
    
    const auto *name_node = dynamic_cast<const ASTString*>(children[size-2]);
    const auto *key = dynamic_cast<const ASTString*>(children[size-1]);
    const auto *key_list = dynamic_cast<const ASTStringList*>(children[size-1]);
    
    // TODO: Validation for [COMPARATOR] / [MATCH-TYPE]
    
    if (name_node == nullptr || (key_list == nullptr && key == nullptr))
        return false;
    
    const auto name = name_node->value();
    
    if (name == "domain" ||
        name == "host" ||
        name == "location" ||
        name == "name" ||
        name == "phase" ||
        name == "remote-host" ||
        name == "remote-ip" ||
        name == "version") {
        return true;
    }
    
    // RFC 6785
    // TODO: Fix this cast
    if (((ASTVerificationVisitor *)visitor())->requires_capability("imapsieve") && (
        name == "imap.user" ||
        name == "imap.email" ||
        name == "imap.cause" ||
        name == "imap.mailbox" ||
        name == "imap.changedflags")) {
        return true;
    }
    
    return false;
}

bool Test::_validateValidExtListTest(const ASTNode *node) {
    const auto *test = dynamic_cast<const ASTTest*>(node);
    
    if (!nodeIsType<ASTStringList>(test->children()[0]) && !nodeIsType<ASTString>(test->children()[0]))
        return false;
    
    return true;
}


}