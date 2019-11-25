/* $Id$
 *
 * ruffina, 2004
 */
#ifndef XMLTABLELOADERPLUGIN_H
#define XMLTABLELOADERPLUGIN_H

#include "plugin.h"
#include "xmltableloader.h"
#include "oneallocate.h"
#include "dlxmlloader.h"

/*
 * XMLTableLoaderPlugin
 */
class XMLTableLoaderPlugin : public virtual XMLTableLoader, public virtual Plugin {
public:

    virtual void initialization( );
    virtual void destruction( );
};

#define TABLE_LOADER(className, tableName, nodeName)                          \
    struct className : public XMLTableLoaderPlugin, public DLXMLTableLoader { \
        virtual DLString getTableName( ) const {                              \
            return tableName;                                                 \
        }                                                                     \
        virtual DLString getNodeName( ) const {                               \
            return nodeName;                                                  \
        }                                                                     \
    }

#define TABLE_LOADER_DECL(className) \
    struct className : public XMLTableLoaderPlugin, public DLXMLTableLoader,  \
                       public OneAllocate  {                                  \
                                                                              \
        className();                                                          \
        virtual ~className();                                                 \
        virtual DLString getTableName( ) const;                               \
        virtual DLString getNodeName( ) const;                                \
    };                                                                        \
    extern className *the ## className;                          

#define TABLE_LOADER_IMPL(className, tableName, nodeName)                     \
className *the ## className = 0;                                              \
className::className() { checkDuplicate(the ## className); the ## className = this; } \
className::~className() { the ## className = 0; }                             \
DLString className::getTableName() const { return tableName; }                \
DLString className::getNodeName() const { return nodeName; }                  


#endif
