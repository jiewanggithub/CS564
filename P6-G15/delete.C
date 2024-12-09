#include "catalog.h"
#include "query.h"


/*
 * Deletes records from a specified relation.
 *
 * Returns:
 * 	OK on success
 * 	an error code otherwise
 */

const Status QU_Delete(const string &relation, 
                       const string &attrName, 
                       const Operator op,
                       const Datatype type, 
                       const char *attrValue) 
{
    cout << "Doing QU_Delete " << endl;

    // Variables for handling the filter and scan
    const char *filter;
    int intVal;
    float floatVal;
    Status status;
    RID recordID;
    AttrDesc attributeDescription;
    HeapFileScan *heapFileScanner;

    // Initialize a heap file scanner for the specified relation
    heapFileScanner = new HeapFileScan(relation, status);
    if (status != OK) {
        return status;
    }

    // Start a scan based on whether a filtering attribute is provided
    if (attrName.empty()) {
        // Perform a full scan with no specific filter
        status = heapFileScanner->startScan(0, 0, type, NULL, op);
    } else {
        // Retrieve attribute details for filtering
        status = attrCat->getInfo(relation, attrName, attributeDescription);
        if (status != OK) {
            return status;
        }

        // Convert the filter value to the appropriate type
        switch (type) {
            case STRING:
                status = heapFileScanner->startScan(attributeDescription.attrOffset, 
                                                    attributeDescription.attrLen, 
                                                    type, 
                                                    attrValue, 
                                                    op);
                break;

            case FLOAT:
                floatVal = atof(attrValue);
                status = heapFileScanner->startScan(attributeDescription.attrOffset, 
                                                    attributeDescription.attrLen, 
                                                    type, 
                                                    (char *)&floatVal, 
                                                    op);
                break;

            case INTEGER:
                intVal = atoi(attrValue);
                status = heapFileScanner->startScan(attributeDescription.attrOffset, 
                                                    attributeDescription.attrLen, 
                                                    type, 
                                                    (char *)&intVal, 
                                                    op);
                break;
        }
    }

    // Check if the scan initialization was successful
    if (status != OK) {
        return status;
    }

    // Iterate through matching records and delete them
    while (heapFileScanner->scanNext(recordID) == OK) {
        status = heapFileScanner->deleteRecord();
        if (status != OK) {
            return status;
        }
    }

    // End the scan and clean up
    status = heapFileScanner->endScan();
    if (status != OK) {
        return status;
    }

    delete heapFileScanner;
    return OK;
}



