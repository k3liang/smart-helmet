#include <Python.h>

int main() {
    Py_Initialize();

    // Set sys.path
    PyRun_SimpleString("import sys; sys.path.append('.')");

    // Import your Python module
    PyObject *pName, *pModule, *pInit, *pDetect, *pCleanup;

    pName = PyUnicode_FromString("eye_detector");
    pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (!pModule) {
        PyErr_Print();
        printf("Failed to load Python module\n");
        return 1;
    }

    // Initialize once
    pInit = PyObject_GetAttrString(pModule, "initialize");
    if (pInit && PyCallable_Check(pInit)) {
        PyObject_CallObject(pInit, NULL);
    }

    // Call repeatedly (your scheduled tasks)
    pDetect = PyObject_GetAttrString(pModule, "detect_drowsiness");
    if (pDetect && PyCallable_Check(pDetect)) {
        PyObject_CallObject(pDetect, NULL);
    }

    // Cleanup after finishing
    pCleanup = PyObject_GetAttrString(pModule, "cleanup");
    if (pCleanup && PyCallable_Check(pCleanup)) {
        PyObject_CallObject(pCleanup, NULL);
    }

    // Clean up Python references
    Py_XDECREF(pInit);
    Py_XDECREF(pDetect);
    Py_XDECREF(pCleanup);
    Py_DECREF(pModule);

    Py_Finalize();
    return 0;
}

