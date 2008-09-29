/***************************************************************************
 *   Copyright (C) 2008 by Andrey Afletdinov                               *
 *   afletdinov@mail.dc.baikal.ru                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "cldap_entry.h"
#include "cldap_server.h"

Ldap::Server::Server() : ldap_object(NULL), ldap_errno(0)
{
}

Ldap::Server::Server(const std::string & connect, bool ssl) : ldap_object(NULL), ldap_errno(0)
{
    ldap_uri = (std::string::npos != connect.find("://") ? connect : (ssl ? "ldaps://" : "ldap://") + connect);
}

Ldap::Server::~Server()
{
    if(ldap_object) Disconnect();
}

const std::string & Ldap::Server::URI(void) const
{
    return ldap_uri;
}

const std::string & Ldap::Server::BindDN(void) const
{
    return ldap_bind_dn;
}

bool Ldap::Server::Connect(const std::string & connect, bool ssl)
{
    if(connect.size())
	ldap_uri = (std::string::npos != connect.find("://") ? connect : (ssl ? "ldaps://" : "ldap://") + connect);

    if(ldap_object) Disconnect();

    ldap_errno = ldap_initialize(&ldap_object, ldap_uri.c_str());

    const int protocol_version = 3;

    if(ldap_object) ldap_set_option(ldap_object, LDAP_OPT_PROTOCOL_VERSION, &protocol_version);

    return LDAP_SUCCESS == ldap_errno;
}

void Ldap::Server::Disconnect(void)
{
    if(ldap_object) Unbind();

    ldap_uri.clear();
}

bool Ldap::Server::Bind(const std::string & bind_dn, const std::string & bind_pw)
{
    ldap_bind_dn = bind_dn;
    ldap_bind_pw = bind_pw;

    return Bind();
}

bool Ldap::Server::Bind(void)
{
    if(!ldap_object) Connect();

    struct berval cred;

    cred.bv_val = const_cast<char *>(ldap_bind_pw.c_str());
    cred.bv_len = ldap_bind_pw.size();

    ldap_errno = ldap_sasl_bind_s(ldap_object, ldap_bind_dn.c_str(), NULL, &cred, NULL, NULL, NULL);

    return LDAP_SUCCESS == ldap_errno;
}

void Ldap::Server::Unbind(void)
{
    if(ldap_object) ldap_errno = ldap_unbind_ext_s(ldap_object, NULL, NULL);

    ldap_bind_dn.clear();
    ldap_bind_pw.clear();

    ldap_object = NULL;
}

bool Ldap::Server::Add(const Entry & entry)
{
    if(!ldap_object) return false;

    Entry & entry2 = const_cast<Entry &>(entry);

    const std::string & dn = entry2.DN();

    return LDAP_SUCCESS == (ldap_errno = ldap_add_ext_s(ldap_object, dn.c_str(), entry2.c_LDAPMod(), NULL, NULL));
}

bool Ldap::Server::Modify(const Entry & entry)
{
    return ldap_object &&
	LDAP_SUCCESS == (ldap_errno = ldap_modify_ext_s(ldap_object, entry.DN().c_str(), const_cast<Entry &>(entry).c_LDAPMod(), NULL, NULL));
}

bool Ldap::Server::Compare(const std::string & attr, const std::string & val) const
{
    return ldap_object &&
	LDAP_COMPARE_TRUE == ldap_compare_ext_s(ldap_object, attr.c_str(), val.c_str(), NULL, NULL, NULL);
}

const char* Ldap::Server::Message(void) const
{
    return ldap_object ? ldap_err2string(ldap_errno) : NULL;
}

bool Ldap::Server::ModDN(const std::string & dn, const std::string & newdn)
{
    return ldap_object &&
	LDAP_SUCCESS == (ldap_errno = ldap_rename_s(ldap_object, dn.c_str(), newdn.c_str(), NULL, 1, NULL, NULL));
}

bool Ldap::Server::Delete(const std::string & dn)
{
    return ldap_object &&
	LDAP_SUCCESS == (ldap_errno = ldap_delete_ext_s(ldap_object, dn.c_str(), NULL, NULL));
}

unsigned int Ldap::Server::Search(Ldap::Entries & result, const std::string & base, scope_t scope, const std::string & filter, const std::list<std::string> & attrs)
{
    if(result.size()) result.clear();

    // prepare ldap attrs
    std::vector<const char *> ldap_attrs;
    const unsigned int & attrs_size = attrs.size();

    if(attrs_size)
    {
	std::list<std::string>::const_iterator it1 = attrs.begin();
	std::list<std::string>::const_iterator it2 = attrs.end();

        for(; it1 != it2; ++it1) ldap_attrs.push_back((*it1).c_str());

        ldap_attrs.push_back(NULL);
    }

    LDAPMessage *res = NULL;
    
    // search
    ldap_errno = ldap_search_ext_s(ldap_object, base.empty() ? NULL : base.c_str(), scope, filter.empty() ? NULL : filter.c_str(), const_cast<char **>(&ldap_attrs[0]), 0, NULL, NULL, NULL, 0, &res);

    if(LDAP_SUCCESS != ldap_errno ||
       0 == ldap_count_entries(ldap_object, res)) return 0;

    for(LDAPMessage *ldap_entry = ldap_first_entry(ldap_object, res); NULL != ldap_entry; ldap_entry = ldap_next_entry(ldap_object, ldap_entry))
    {
        char *dn = ldap_get_dn(ldap_object, ldap_entry);

        result.push_back(Entry());
    
        Entry & current_entry = result.back();
        current_entry.DN(std::string(dn));

	BerElement *ber = NULL;

    	for(char *ldap_attr = ldap_first_attribute(ldap_object, ldap_entry, &ber); NULL != ldap_attr; ldap_attr = ldap_next_attribute(ldap_object, ldap_entry, ber))
    	{
    	    berval **vals = ldap_get_values_len(ldap_object, ldap_entry, ldap_attr);
    	    const std::string attr(ldap_attr);

    	    for(int i = 0; NULL != vals[i]; ++i) if(vals[i]->bv_val && vals[i]->bv_len) current_entry.Add(attr, std::string(vals[i]->bv_val));

    	    ldap_value_free_len(vals);
    	    ldap_memfree(ldap_attr);
    	}

    	if(ber) ber_free(ber, 0);
        if(dn) ldap_memfree(dn);
    }

    if(res) ldap_msgfree(res);

    return result.size();
}

bool Ldap::Server::Ping(void) const
{
    if(!ldap_object) return false;

    LDAPMessage *res = NULL;

    int errno = ldap_search_ext_s(ldap_object, NULL, BASE, NULL, NULL, 0, NULL, NULL, NULL, 0, &res);

    if(res) ldap_msgfree(res);

    return LDAP_SERVER_DOWN != errno;
}

const LDAP * Ldap::Server::c_LDAP(void) const
{
    return ldap_object;
}

int Ldap::Server::Error(void) const
{
    return ldap_errno;
}
