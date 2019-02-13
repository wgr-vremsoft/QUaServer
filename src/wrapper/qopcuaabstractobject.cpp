#include "qopcuaabstractobject.h"

QOpcUaAbstractObject::QOpcUaAbstractObject(QOpcUaServerNode *parent) : QOpcUaServerNode(parent)
{
	// Set NodeId manually only for pre-defined types. 
	// For custom types, the type NodeId is set upon registering the class in the server
}

QOpcUaAbstractObject::QOpcUaAbstractObject(QOpcUaServer *server) : QOpcUaServerNode(server)
{
	// This private method exists so QOpcUaServer can create the UA_NS0ID_OBJECTSFOLDER instance
}