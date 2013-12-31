/*******************************************************************************
 * Copyright 2013 Aerospike Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy 
 * of this software and associated documentation files (the "Software"), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or 
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in 
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 ******************************************************************************/

extern "C" {
#include <aerospike/aerospike.h>
#include <aerospike/aerospike_key.h>
#include <aerospike/as_config.h>
#include <aerospike/as_key.h>
#include <aerospike/as_record.h>
#include <aerospike/aerospike_batch.h>
}

#include <node.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <vector>

#include "../client.h"
#include "../util/async.h"
#include "../util/conversions.h"
#include "../util/log.h"

#define BGET_ARG_POS_KEY     0
#define BGET_ARG_POS_BPOLICY 1 // Batch policy position and callback position is not same 
#define BGET_ARG_POS_CB      2 // in the argument list for every invoke of batch_get. If 
// writepolicy is not passed from node application, argument 
// position for callback changes.
using namespace v8;

/*******************************************************************************
 *      TYPES
 ******************************************************************************/

/**
 *      AsyncData — Data to be used in async calls.
 */
typedef struct AsyncData {
    aerospike * as;
    int node_err;            // To Keep track of the parameter errors from Nodejs 
    as_error err;
    as_policy_batch policy;
    as_batch batch;          // Passed as input to aerospike_batch_get
    as_batch_read  *results; // Results from a aerospike_batch_get operation
    uint32_t n;
    AerospikeClient * client;
    Persistent<Function> callback;
} AsyncData;



/*******************************************************************************
 *      FUNCTIONS
 ******************************************************************************/

bool batch_callback(const as_batch_read * results, uint32_t n, void * udata)
{
    // Fetch the AsyncData structure
    AsyncData *     data    = reinterpret_cast<AsyncData *>(udata);
    LogInfo * log = &data->client->log;
    //copy the batch result to the shared data structure AsyncData,
    //so that response can send it back to nodejs layer
    //as_batch_read  *batch_result = &data->results;
    if( results != NULL ) {
        as_v8_debug(log, "Bridge callback invoked in V8 for the batch request of %d records ", n);
        data->n = n;
        data->results = (as_batch_read *)calloc(n, sizeof(as_batch_read));
        for ( uint32_t i = 0; i < n; i++ ) {
            data->results[i].result = results[i].result; 
            if (results[i].result == AEROSPIKE_OK) {            
                as_record * rec = NULL ;
                rec = &data->results[i].record;

                as_v8_detail(log, "Record[%d]", i);
                DETAIL(log, BINS, &results[i].record);
                DETAIL(log, META, &results[i].record);
                as_v8_debug(log, "record returned for the key");
                DEBUG(log, _KEY, results[i].key);

                as_record_init(rec, results[i].record.bins.size);
                key_clone(results[i].key, (as_key**) &data->results[i].key, log); 
                record_clone(&results[i].record, &rec, log);
            } 
        }
        return true;
    }
    else {
        as_v8_info(log, "Brigde callback in v8 for batch called with no batch results");
        data->n = 0;
        data->results = NULL;
    }
    return false;
}
/**
 *      prepare() — Function to prepare AsyncData, for use in `execute()` and `respond()`.
 *  
 *      This should only keep references to V8 or V8 structures for use in 
 *      `respond()`, because it is unsafe for use in `execute()`.
 */
static void * prepare(const Arguments& args)
{
    // The current scope of the function
    HandleScope scope;

    AerospikeClient * client = ObjectWrap::Unwrap<AerospikeClient>(args.This());

    // Build the async data
    AsyncData *     data = new AsyncData;
    data->as = &client->as;
    data->node_err = 0;
    data->n = 0;
    data->results = NULL;
    data->client  = client;
    // Local variables
    as_batch * batch = &data->batch;
    as_policy_batch * policy = &data->policy;

    int arglength = args.Length();

    LogInfo * log = &client->log;

    if ( args[arglength-1]->IsFunction()) { 
        data->callback = Persistent<Function>::New(Local<Function>::Cast(args[arglength-1]));   
        as_v8_detail(log, "batch_get callback registered");
    } else {
        as_v8_error(log, "Arglist must contain a callback function");
        COPY_ERR_MESSAGE( data->err, AEROSPIKE_ERR_PARAM);
        goto Err_Return;
    }

    if ( args[BGET_ARG_POS_KEY]->IsArray() ) {
        Local<Array> keys = Local<Array>::Cast(args[BGET_ARG_POS_KEY]);
        if( batch_from_jsarray(batch, keys, log) != AS_NODE_PARAM_OK) {
            as_v8_error(log, "parsing batch keys failed");
            COPY_ERR_MESSAGE( data->err, AEROSPIKE_ERR_PARAM);
            goto Err_Return;
        }
    }
    else {
        //Parameter passed is not an array of Key Objects "ERROR..!"
        as_v8_error(log, "Batch key must be an array of key objects");
        COPY_ERR_MESSAGE( data->err, AEROSPIKE_ERR_PARAM);
        goto Err_Return;
    }

    if (arglength > 2 ) {
        if ( args[BGET_ARG_POS_BPOLICY]->IsObject() ) {
            if (batchpolicy_from_jsobject(policy, args[BGET_ARG_POS_BPOLICY]->ToObject(), log) != AS_NODE_PARAM_OK) {
                as_v8_error(log, "Parsing batch policy failed");
                COPY_ERR_MESSAGE( data->err, AEROSPIKE_ERR_PARAM);
                goto Err_Return;
            }
        }else {
            as_v8_error(log, "Batch policy must be an object");
            COPY_ERR_MESSAGE( data->err, AEROSPIKE_ERR_PARAM);
            goto Err_Return;
        }
    } else {
        as_v8_detail(log, "Arglist does not contain batch policy, using default values");
        as_policy_batch_init(policy);
    }

    return data;

Err_Return:
    data->node_err = 1;
    return data;
}
/**
 *      execute() — Function to execute inside the worker-thread.
 *  
 *      It is not safe to access V8 or V8 data structures here, so everything
 *      we need for input and output should be in the AsyncData structure.
 */
static void execute(uv_work_t * req)
{
    // Fetch the AsyncData structure
    AsyncData * data = reinterpret_cast<AsyncData *>(req->data);

    // Data to be used.
    aerospike *     as      = data->as;
    as_error  *     err     = &data->err;
    as_batch  *     batch   = &data->batch;
    as_policy_batch * policy= &data->policy;
    LogInfo * log           = &data->client->log;

    if( as->cluster == NULL) {
        as_v8_error(log, "Cluster Object is NULL, can't perform the operation");
        data->node_err = 1;
        COPY_ERR_MESSAGE(data->err, AEROSPIKE_ERR_PARAM);
    }
    // Invoke the blocking call.
    // Check for no parameter errors from Nodejs 
    if( data->node_err == 0) {
        as_v8_debug(log, "Submitting batch request to server with %d keys", batch->keys.size);
        aerospike_batch_get(as, err, policy, batch, batch_callback, (void*) req->data);
        if( err->code != AEROSPIKE_OK) {
            DEBUG(log, ERROR, err);
            data->results = NULL;
            data->n = 0;
        }
        as_batch_destroy(batch);
    }

}


/**
 *  respond() — Function to be called after `execute()`. Used to send response
 *  to the callback.
 *  
 *  This function will be run inside the main event loop so it is safe to use 
 *  V8 again. This is where you will convert the results into V8 types, and 
 *  call the callback function with those results.
 */
static void respond(uv_work_t * req, int status)
{
    // Scope for the callback operation.
    HandleScope scope;

    // Fetch the AsyncData structure
    AsyncData * data    = reinterpret_cast<AsyncData *>(req->data);
    as_error *  err     = &data->err;
    uint32_t num_rec = data->n;
    as_batch_read* batch_results = data->results;

    LogInfo * log = &data->client->log;

    // maintain a linked list of pointers to be freed after the nodejs callback is called
    // Buffer object is not garbage collected by v8 gc. Have to delete explicitly 
    // to avoid memory leak.

    // Build the arguments array for the callback
    int num_args = 2;
    Handle<Value> argv[num_args] ;
    Handle<Array> arr;
    as_v8_debug(log, "Batch Get: the response is");
    DEBUG(log, ERROR, err);
    int rec_found = 0;

    if(data->node_err == 1) {
        // Sets the err->code and err->message in the 'err' variable
        err->func = NULL;
        err->line = 0;
        err->file = NULL;
        argv[0] = error_to_jsobject(err, log);
        argv[1] = Null();
        argv[2] = Null();
    }else if (num_rec == 0 || batch_results == NULL) {
        argv[0] = error_to_jsobject(err, log);
        argv[1] = Null();
        argv[2] = Null();
    }
    else {
        arr=Array::New(num_rec);    
        for ( uint32_t i = 0; i< num_rec; i++) {
            Handle<Object> obj = Object::New();
            obj->Set(String::NewSymbol("recstatus"), Integer::New(batch_results[i].result));
            if(batch_results[i].result == AEROSPIKE_OK) {   
                obj->Set(String::NewSymbol("record"),record_to_jsobject( &batch_results[i].record, batch_results[i].key, log ));
                as_key_destroy((as_key*) batch_results[i].key);
                as_record_destroy(&batch_results[i].record);
                rec_found++;
            } else {
                as_v8_debug(log, "Record[%d] not returned by server ", i);
            }

            arr->Set(i,obj);
        }
        as_v8_debug(log, "%d record objects are present in the batch array",  rec_found);
        argv[0] = error_to_jsobject(err, log);
        argv[1] = arr;  
    }

    // Surround the callback in a try/catch for safety`
    TryCatch try_catch;

    // Execute the callback.
    data->callback->Call(Context::GetCurrent()->Global(), num_args, argv);

    // Process the exception, if any
    if ( try_catch.HasCaught() ) {
        node::FatalException(try_catch);
    }

    as_v8_debug(log,"Invoked the callback");
    // Dispose the Persistent handle so the callback
    // function can be garbage-collected
    data->callback.Dispose();

    // clean up any memory we allocated
    if ( data->node_err == 1) {
        free(data->results);    
    }
    if (batch_results != NULL) {
        free(batch_results);
    }

    as_v8_debug(log, "Cleaned up the resources");


    delete data;
    delete req;
    scope.Close(Undefined());
}

/*******************************************************************************
 *  OPERATION
 ******************************************************************************/

/**
 *      The 'batch_get()' Operation
 */
Handle<Value> AerospikeClient::Batch_Get(const Arguments& args)
{
    return async_invoke(args, prepare, execute, respond);
}

