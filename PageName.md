# LDAP c++ simple wrapper #

```cpp

Ldap::Server ldap;

ldap.Connect(uri);
std::cout << "ldap connect: " <<  ldap.Message() << std::endl;

bool success = false;
success = ldap.Bind(bind_dn, bind_pw);

std::cout << "ldap bind: " <<  ldap.Message() << std::endl;
if(! success) return 1;

const std::string & test_ou = "cldap_test_ou";
const std::string object_dn("ou=" + test_ou + "," + bdn);
Ldap::Entry entry(object_dn);

entry.Append(Ldap::ADD, "objectClass", "top");
entry.Append(Ldap::ADD, "objectClass", "organizationalUnit");
entry.Append(Ldap::ADD, "ou", test_ou);

std::list<std::string> vals;
vals.push_back("this test organizational unit block");
vals.push_back("addons description line 2");
vals.push_back("addons description line 3");

entry.Append(Ldap::ADD, "description", "vals 1 2 3");
entry.Append(Ldap::ADD, "description", vals);
std::cout << std::endl << "entry dump: " <<  std::endl << entry << std::endl << std::endl;

// update
success = ldap.Add(entry);
std::cout << "ldap add: " <<  ldap.Message() << std::endl;
if(success)
{
Ldap::Entry entry2(entry.DN());
entry2.Append(Ldap::REPLACE, "description", "vals ssssss");
success = ldap.Modify(entry2);
std::cout << "ldap modify: " <<  ldap.Message() << std::endl;
if(! success) return 3;
}

// search
Ldap::Entries result = ldap.Search(object_dn, Ldap::BASE);
std::cout << std::endl << "ldap search: " << object_dn << std::endl << "found: " << result.size() << " entries." << std::endl << std::endl;
if(result.size())
{
std::cout << result;

// delete
success = ldap.Delete(object_dn);
std::cout << "ldap delete: " << ldap.Message() << std::endl << std::endl;
}
```