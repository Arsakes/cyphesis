#include <stdio.h>
#include <unistd.h>

#include <Python.h>

#include "Python_API.h"
#include "Thing.h"
#include <modules/Location.h>


void Create_PyThing(Thing * thing, const string & package, const string & type)
{
    PyObject * mod_dict;
    if ((mod_dict = PyImport_ImportModule((char *)package.c_str()))==NULL) {
        cout << "Cld no find python module " << package << endl << flush;
            PyErr_Print();
        return;
    } else {
        cout << "Got python module " << package << endl << flush;
    }
    PyObject * my_class = PyObject_GetAttrString(mod_dict, (char *)type.c_str());
    if (my_class == NULL) {
        cout << "Cld no find class in module " << package << endl << flush;
            PyErr_Print();
        return;
    } else {
        cout << "Got python class " << type << " in " << package << endl << flush;
    }
    if (PyCallable_Check(my_class) == 0) {
            cout << "It does not seem to be a class at all" << endl << flush;
        return;
    }
    ThingObject * pyThing = newThingObject(NULL);
    pyThing->m_thing = thing;
    if (thing->set_object(PyEval_CallFunction(my_class,"(O)", (PyObject *)pyThing)) == -1) {
        if (PyErr_Occurred() == NULL) {
            cout << "Could not get python obj" << endl << flush;
        } else {
            cout << "Reporting python error for " << type << endl << flush;
            PyErr_Print();
        }
    }
}

static PyObject * location_new(PyObject * self, PyObject * args)
{
	LocationObject *o;
	// We need to deal with actual args here
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newLocationObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	o->location = new Location;
	return (PyObject *)o;
}

static PyObject * vector3d_new(PyObject * self, PyObject * args)
{
	Vector3DObject *o;
	// We need to deal with actual args here
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newVector3DObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	return (PyObject *)o;
}

static PyObject * object_new(PyObject * self, PyObject * args)
{
	AtlasObject *o;
	
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newAtlasObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	o->m_obj = new Object;
	return (PyObject *)o;
}

static PyObject * cppthing_new(PyObject * self, PyObject * args)
{
	ThingObject *o;
	
	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	o = newThingObject(args);
	if ( o == NULL ) {
		return NULL;
	}
	//o->m_thing = new Thing;
	return (PyObject *)o;
}

static PyObject * operation_new(PyObject * self, PyObject * args)
{
	RootOperationObject * op;

	if (!PyArg_ParseTuple(args, "")) {
		return NULL;
	}
	op = newAtlasRootOperation(args);
	if (op == NULL) {
		return NULL;
	}
	op->operation = new RootOperation;
	*op->operation = RootOperation::Instantiate();
	return (PyObject *)op;
}

static PyMethodDef atlas_methods[] = {
	/* {"system",	spam_system, METH_VARARGS}, */
	{"Operation",	operation_new,	METH_VARARGS},
	{"Location",	location_new,	METH_VARARGS},
	{"Object",	object_new,	METH_VARARGS},
	{"cppThing",	cppthing_new,	METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

static PyMethodDef Vector3D_methods[] = {
	{"Vector3D",	vector3d_new,	METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

static PyMethodDef server_methods[] = {
	//{"null",	null_new,	METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

static PyMethodDef common_methods[] = {
	//{"null",	null_new,	METH_VARARGS},
	{NULL,		NULL}				/* Sentinel */
};

void init_python_api()
{
	char * cwd;

	if ((cwd = getcwd(NULL, 0)) != NULL) {
                size_t len = strlen(cwd) + 12;
                char * pypath = (char *)malloc(len);
                strcpy(pypath, cwd);
                strcat(pypath, "/rulesets/basic");
		setenv("PYTHONPATH", pypath, 1);
	}

	Py_Initialize();

	if (Py_InitModule("atlas", atlas_methods) == NULL) {
		printf("Failed to Create atlas thing\n");
		return;
	}
	printf("Created atlas thing\n");

	if (Py_InitModule("Vector3D", Vector3D_methods) == NULL) {
		printf("Failed to Create Vector3D thing\n");
		return;
	}
	printf("Created Vector3D thing\n");

	PyObject * common;
	PyObject * dict;
	if ((common = Py_InitModule("common", common_methods)) == NULL) {
		printf("Failed to Create common thing\n");
		return;
	}
	printf("Created common thing\n");
	PyObject * _const = PyModule_New("const");
	PyObject * log = PyModule_New("log");
	dict = PyModule_GetDict(common);
	PyDict_SetItemString(dict, "const", _const);
	PyDict_SetItemString(dict, "log", log);
	PyObject_SetAttrString(_const, "server_python", PyInt_FromLong(0));

	PyObject * server;
	if ((server = Py_InitModule("server", server_methods)) == NULL) {
		printf("Failed to Create server thing\n");
		return;
	}
	dict = PyModule_GetDict(server);
	PyObject * dictlist = PyModule_New("dictlist");
	PyDict_SetItemString(dict, "dictlist", dictlist);
	if (PyExc_IOError != NULL) {
		printf("Got PyExc_IOError\n");
	}
}
