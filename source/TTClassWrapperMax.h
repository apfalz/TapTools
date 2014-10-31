/* 
 *	TTClassWrapperMax
 *	An automated class wrapper to make Jamoma object's available as objects for Max/MSP
 *	Copyright © 2008 by Timothy Place
 * 
 * License: This code is licensed under the terms of the "New BSD License"
 * http://creativecommons.org/licenses/BSD/
 */

#ifndef __TT_CLASS_WRAPPER_MAX_H__
#define __TT_CLASS_WRAPPER_MAX_H__

#include "ext.h"					// Max Header
#include "z_dsp.h"					// MSP Header
#include "ext_strings.h"			// String Functions
#include "commonsyms.h"				// Common symbols used by the Max 4.5 API
#include "ext_obex.h"				// Max Object Extensions (attributes) Header
#include "TTFoundationAPI.h"		// Jamoma Foundation API
#include "TTDSP.h"					// Jamoma DSP API

// TYPE DEFINITIONS

typedef TTErr (*TTValidityCheckFunction)(const TTPtr data);		///< A type that can be used to store a pointer to a validity checking function.


#ifndef SELF
#define SELF ((t_object*)self)
#endif


class WrappedClassOptions;

typedef struct _wrappedClass {
	t_class*				maxClass;							///< The Max class pointer.
	t_symbol*				maxClassName;						///< The name to give the Max class.
	TTSymbol				ttblueClassName;					///< The name of the class as registered with the TTBlue framework.
	TTValidityCheckFunction validityCheck;						///< A function to call to validate the context for an object before it is instantiated.
	TTPtr					validityCheckArgument;				///< An argument to pass to the validityCheck function when it is called.
	WrappedClassOptions*	options;							///< Additional configuration options specified for the class.
	t_hashtab*				maxNamesToTTNames;					///< names may not be direct mappings, as we downcase the first letter.
	
	void*					specificities;						///< anything needed to deal with somes specificities of the class...
} WrappedClass;



class WrappedClassOptions {
protected:
	TTHash*	options;

public:
	WrappedClassOptions()
	{
		options = new TTHash;
	}
	
	virtual ~WrappedClassOptions()
	{
		delete options;
	}
	
	TTErr append(const TTSymbol& optionName, const TTValue& optionValue)
	{
		return options->append(optionName, optionValue);
	}
	
	/**	Returns an error if the requested option doesn't exist. */
	TTErr lookup(const TTSymbol& optionName, TTValue& optionValue)
	{
		return options->lookup(optionName, optionValue);
	}
	
};
	

typedef WrappedClass* WrappedClassPtr;							///< A pointer to a WrappedClass.
typedef WrappedClassOptions* WrappedClassOptionsPtr;			///< A pointer to WrappedClassOptions.




// Data Structure for this object
typedef struct _wrappedInstance {
    t_pxobject 		obj;						///< Max audio object header
	WrappedClassPtr	wrappedClassDefinition;		///< A pointer to the class definition
	TTAudioObject*	wrappedObject;				///< The instance of the TTBlue object we are wrapping
	TTAudio*		audioIn;					///< Audio input signal
	TTAudio*		audioOut;					///< Audio output signal
	TTUInt16		vs;
	TTUInt16		maxNumChannels;				///< The number of channels for which this object is initialized to operate upon
	TTUInt16		numChannels;				///< The actual number of channels in use
	TTUInt16		numInputs;
	TTUInt16		numOutputs;
	TTUInt16		numControlSignals;			///< What number, a subset of numInputs, of signals are for controlling attributes?
	TTSymbol*		controlSignalNames;			///< An array of attribute names for the control signals.
    
    TTPtr           controlOutlet;              ///< for output from a notification
    TTObject*		controlCallback;            ///< for output from a notification
	
	t_bool			signals_connected[256];		///< which inlets and outlets are actually connected to audio signals
} WrappedInstance;

typedef WrappedInstance* WrappedInstancePtr;	///< Pointer to a wrapped instance of our object.





// FUNCTIONS


// private:
// self has to be TTPtr because different wrappers (such as the ui wrapper) have different implementations.
t_max_err wrappedClass_attrGet(TTPtr self, t_object* attr, long* argc, t_atom** argv);
t_max_err wrappedClass_attrSet(TTPtr self, t_object* attr, long argc, t_atom* argv);
void wrappedClass_anything(TTPtr self, t_symbol* s, long argc, t_atom* argv);
void wrappedClass_assist(WrappedInstancePtr self, void *b, long msg, long arg, char *dst);




// public:

// Wrap a Jamoma class as a Max class.
TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c);

// This version can be passed a method that is called to make sure it is legit to instantiate the class.
TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck);

// This version can be passed a method that is called to make sure it is legit to instantiate the class.
TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, TTPtr validityCheckArgument);


// These are versions of the above, but for which additional options can be specified.
TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, WrappedClassOptionsPtr options);
TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, WrappedClassOptionsPtr options);
TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, TTPtr validityCheckArgument, WrappedClassOptionsPtr options);

TTErr TTValueFromAtoms(TTValue& v, long ac, t_atom* av);
TTErr TTAtomsFromValue(const TTValue& v, long* ac, t_atom** av); // NOTE: allocates memory

long TTMatrixReferenceJitterMatrix(TTMatrix aMatrix, TTPtr aJitterMatrix, TTBoolean copy = true);
TTErr TTMatrixCopyDataFromJitterMatrix(TTMatrix aMatrix, TTPtr aJitterMatrix);
TTErr TTMatrixCopyDataToJitterMatrix(TTMatrix aMatrix, TTPtr aJitterMatrix);

#endif // __TT_CLASS_WRAPPER_MAX_H__
