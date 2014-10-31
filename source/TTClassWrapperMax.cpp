/* 
 *	TTClassWrapperMax
 *	An automated class wrapper to make Jamoma objects available as objects for Max/MSP
 *	Copyright � 2008 by Timothy Place
 * 
 * License: This code is licensed under the terms of the "New BSD License"
 * http://creativecommons.org/licenses/BSD/
 */

#include "TTClassWrapperMax.h"
#include "ext_hashtab.h"
#include "TTCallback.h"
#ifdef MAC_VERSION
#include <dlfcn.h>
#endif

extern "C" void wrappedClass_receiveNotificationForOutlet(WrappedInstancePtr self, TTValue& arg);


/** A hash of all wrapped clases, keyed on the Max class name. */
static t_hashtab*	wrappedMaxClasses = NULL;


t_object* wrappedClass_new(t_symbol* name, long argc, t_atom* argv)
{	
	WrappedClass*		wrappedMaxClass = NULL;
    WrappedInstancePtr	x = NULL;
	TTValue				sr(sys_getsr());
	TTValue				v, none;
 	long				attrstart = attr_args_offset(argc, argv);		// support normal arguments
	TTErr				err = kTTErrNone;
	
	// Find the WrappedClass
	hashtab_lookup(wrappedMaxClasses, name, (t_object**)&wrappedMaxClass);
	
	// If the WrappedClass has a validity check defined, then call the validity check function.
	// If it returns an error, then we won't instantiate the object.
	if (wrappedMaxClass) {
		if (wrappedMaxClass->validityCheck)
			err = wrappedMaxClass->validityCheck(wrappedMaxClass->validityCheckArgument);
		else
			err = kTTErrNone;
	}
	else
		err = kTTErrGeneric;
	
	if (!err)
		x = (WrappedInstancePtr)object_alloc(wrappedMaxClass->maxClass);
    if (x) {
		x->wrappedClassDefinition = wrappedMaxClass;
		x->maxNumChannels = 2;		// An initial argument to this object will set the maximum number of channels
		if (attrstart && argv)
			x->maxNumChannels = atom_getlong(argv);
		
		ttEnvironment->setAttributeValue(kTTSym_sampleRate, sr);
		
		if (wrappedMaxClass->options && !wrappedMaxClass->options->lookup(TT("numChannelsUseFixedRatioInputsToOutputs"), v)) {
		   TTUInt16	inputs;
		   TTUInt16	outputs;
		   
		   inputs = v[0];
		   outputs = v[1];
		   x->numInputs = x->maxNumChannels * inputs;
		   x->numOutputs = x->maxNumChannels * outputs;
		}
		else if (wrappedMaxClass->options && !wrappedMaxClass->options->lookup(TT("fixedNumChannels"), v)) {
			TTUInt16 numChannels;
			
			numChannels = v[0];
			x->numInputs = numChannels;
			x->numOutputs = numChannels;
		}
		else if (wrappedMaxClass->options && !wrappedMaxClass->options->lookup(TT("fixedNumOutputChannels"), v)) {
			TTUInt16 numChannels;
			
			numChannels = v[0];
			x->numInputs = x->maxNumChannels;
			x->numOutputs = numChannels;
		}
		else {
		   x->numInputs = x->maxNumChannels;
		   x->numOutputs = x->maxNumChannels;
		}
		
		if (wrappedMaxClass->options && !wrappedMaxClass->options->lookup(TT("additionalSignalInputSetsAttribute"), v)) {
			x->numControlSignals = v.size();
			x->controlSignalNames = new TTSymbol[x->numControlSignals];
			for (TTUInt16 i=0; i<x->numControlSignals; i++) {
				x->numInputs++;
				x->controlSignalNames[i] = v[i];
			}
		}
		
		x->wrappedObject = new TTAudioObject(wrappedMaxClass->ttblueClassName, x->maxNumChannels);
		x->audioIn = new TTAudio(x->numInputs);
		x->audioOut = new TTAudio(x->numOutputs);
		attr_args_process(x,argc,argv);				// handle attribute args			
    	object_obex_store((void *)x, _sym_dumpout, (object *)outlet_new(x,NULL));	// dumpout
		
		
		dsp_setup((t_pxobject *)x, x->numInputs);			// inlets
				
		//if (wrappedMaxClass->options && !wrappedMaxClass->options->lookup(TT("numControlOutlets"), v))
		//	v.get(0, numControlOutlets);
		if (wrappedMaxClass->options && !wrappedMaxClass->options->lookup(TT("controlOutletFromNotification"), v)) {
            TTUInt16    outletIndex = 0;
            TTSymbol	notificationName;
            
 			outletIndex = v[0];
 			notificationName = v[1];
            
            // TODO: to support more than one notification->outlet we need see how many args are actually passed-in
            // and then we need to track them in a hashtab or something...
            
            x->controlOutlet = outlet_new((t_pxobject*)x, NULL);
            
            x->controlCallback = new TTObject("callback");
            x->controlCallback->set("function", TTPtr(&wrappedClass_receiveNotificationForOutlet));
            x->controlCallback->set("baton", TTPtr(x));	
 
        	// dynamically add a message to the callback object so that it can handle the 'objectFreeing' notification
            x->controlCallback->instance()->registerMessage(notificationName, (TTMethod)&TTCallback::notify, kTTMessagePassValue);
            
            // tell the source that is passed in that we want to watch it
            x->wrappedObject->registerObserverForNotifications(*x->controlCallback);

        }
        
		for (short i=0; i < x->numOutputs; i++)
			outlet_new((t_pxobject *)x, "signal");			// outlets

		  
		x->obj.z_misc = Z_NO_INPLACE;
	}
	return (t_object*)x;
}


void wrappedClass_free(WrappedInstancePtr x)
{
	dsp_free((t_pxobject *)x);
	delete x->wrappedObject;
	delete x->audioIn;
	delete x->audioOut;
	delete[] x->controlSignalNames;
}


void wrappedClass_receiveNotificationForOutlet(WrappedInstancePtr self, TTValue& arg)
{
    TTString	string = arg[0];
    t_symbol*   s = gensym((char*)string.c_str());
    
    outlet_anything(self->controlOutlet, s, 0, NULL);
}


t_max_err wrappedClass_attrGet(TTPtr self, t_object* attr, long* argc, t_atom** argv)
{
	t_symbol*	attrName = (t_symbol*)object_method(attr, _sym_getname);
	TTValue		v;
	long	i;
	WrappedInstancePtr x = (WrappedInstancePtr)self;
	TTPtr		rawpointer;
	t_max_err		err;
	
	err = hashtab_lookup(x->wrappedClassDefinition->maxNamesToTTNames, attrName, (t_object**)&rawpointer);
	if (err)
		return err;

	TTSymbol	ttAttrName(rawpointer);
	
	x->wrappedObject->get(ttAttrName, v);

	*argc = v.size();
	if (!(*argv)) // otherwise use memory passed in
		*argv = (t_atom *)sysmem_newptr(sizeof(t_atom) * v.size());

	for (i=0; i<v.size(); i++) {
		if (v[i].type() == kTypeFloat32 || v[i].type() == kTypeFloat64) {
			TTFloat64	value;
			value = v[i];
			atom_setfloat(*argv+i, value);
		}
		else if (v[i].type() == kTypeSymbol) {
			TTSymbol	value;
			value = v[i];
			atom_setsym(*argv+i, gensym((char*)value.c_str()));
		}
		else {	// assume int
			TTInt32		value;
			value = v[i];
			atom_setlong(*argv+i, value);
		}
	}	
	return MAX_ERR_NONE;
}


t_max_err wrappedClass_attrSet(TTPtr self, t_object* attr, long argc, t_atom* argv)
{
	WrappedInstancePtr x = (WrappedInstancePtr)self;
	
	if (argc && argv) {
		t_symbol*	attrName = (t_symbol*)object_method(attr, _sym_getname);
		TTValue		v;
		long	i;
		t_max_err		err;
		TTPtr		ptr = NULL;
		
		err = hashtab_lookup(x->wrappedClassDefinition->maxNamesToTTNames, attrName, (t_object**)&ptr);
		if (err)
			return err;
		
		TTSymbol	ttAttrName(ptr);
		
		v.resize(argc);
		for (i=0; i<argc; i++) {
			if (atom_gettype(argv+i) == A_LONG)
				v[i] = (TTInt32)atom_getlong(argv+i);
			else if (atom_gettype(argv+i) == A_FLOAT)
				v[i] = atom_getfloat(argv+i);
			else if (atom_gettype(argv+i) == A_SYM)
				v[i] = TT(atom_getsym(argv+i)->s_name);
			else
				object_error((t_object*)x, "bad type for attribute setter");
		}
		x->wrappedObject->set(ttAttrName, v);
		return MAX_ERR_NONE;
	}
	return MAX_ERR_GENERIC;
}


void wrappedClass_anything(TTPtr self, t_symbol* s, long argc, t_atom* argv)
{
	WrappedInstancePtr	x = (WrappedInstancePtr)self;
	TTSymbol			ttName;
	t_max_err				err;
	TTValue				v_in;
	TTValue				v_out;
	
	err = hashtab_lookup(x->wrappedClassDefinition->maxNamesToTTNames, s, (t_object**)&ttName);
	if (err) {
		object_post((t_object*)x, "no method found for %s", s->s_name);
		return;
	}

	if (argc && argv) {
		v_in.resize(argc);
		for (long i=0; i<argc; i++) {
			if (atom_gettype(argv+i) == A_LONG)
				v_in[i] = (TTInt32)atom_getlong(argv+i);
			else if (atom_gettype(argv+i) == A_FLOAT)
				v_in[i] = atom_getfloat(argv+i);
			else if (atom_gettype(argv+i) == A_SYM)
				v_in[i] = TT(atom_getsym(argv+i)->s_name);
			else
				object_error((t_object*)x, "bad type for message arg");
		}
	}
	x->wrappedObject->send(ttName, v_in, v_out);
		
	// process the returned value for the dumpout outlet
	{
		long	ac = v_out.size();

		if (ac) {
			t_atom*		av = (t_atom*)malloc(sizeof(t_atom) * ac);
			
			for (long i=0; i<ac; i++) {
				if (v_out[0].type() == kTypeSymbol) {
					TTSymbol ttSym;
					ttSym = v_out[i];
					atom_setsym(av+i, gensym((char*)ttSym.c_str()));
				}
				else if (v_out[0].type() == kTypeFloat32 || v_out[0].type() == kTypeFloat64) {
					TTFloat64 f = 0.0;
					f = v_out[i];
					atom_setfloat(av+i, f);
				}
				else {
					TTInt32 l = 0;
					l = v_out[i];
					atom_setfloat(av+i, l);
				}
			}
			object_obex_dumpout(self, s, ac, av);
			free(av);
		}
	}
}


// Method for Assistance Messages
void wrappedClass_assist(WrappedInstancePtr self, void *b, long msg, long arg, char *dst)
{
	if (msg==1)	{		// Inlets
		if (arg==0)
			strcpy(dst, "signal input, control messages"); //leftmost inlet
		else { 
			if (arg > self->numInputs-self->numControlSignals-1)
				//strcpy(dst, "control signal input");		
				snprintf(dst, 256, "control signal for \"%s\"", self->controlSignalNames[arg - self->numInputs+1].c_str());
			else
				strcpy(dst, "signal input");		
		}
	}
	else if (msg==2)	{	// Outlets
		if (arg < self->numOutputs)
			strcpy(dst, "signal output");
		else
			strcpy(dst, "dumpout"); //rightmost outlet
	}
}


void wrappedClass_perform64(WrappedInstancePtr self, t_object* dsp64, double **ins, long numins, double **outs, long numouts, long sampleframes, long flags, void *userparam)
{
	TTUInt16 i;
	//TTUInt16 numChannels = numouts;
	
	self->numChannels = numouts; // <-- this is kinda lame, but for the time being I think we can get away with this assumption...
	
	for (i=0; i < self->numControlSignals; i++) {
		int signal_index = self->numInputs - self->numControlSignals + i;
		
		if (self->signals_connected[signal_index])
			self->wrappedObject->set(self->controlSignalNames[i], *ins[signal_index]);
	}
	
	self->audioIn->setNumChannels(self->numInputs-self->numControlSignals);
	self->audioOut->setNumChannels(self->numOutputs);
	self->audioOut->allocWithVectorSize(sampleframes);
	
	for (i=0; i < self->numInputs-self->numControlSignals; i++)
		self->audioIn->setVector(i, self->vs, ins[i]);
	
	self->wrappedObject->process(self->audioIn, self->audioOut);
	
	for (i=0; i < self->numOutputs; i++) 
		self->audioOut->getVectorCopy(i, self->vs, outs[i]);
	
}


void wrappedClass_dsp64(WrappedInstancePtr self, t_object* dsp64, short *count, double samplerate, long maxvectorsize, long flags)
{
	for (int i=0; i < (self->numInputs + self->numOutputs); i++)
		self->signals_connected[i] = count[i];
	
	ttEnvironment->setAttributeValue(kTTSym_sampleRate, samplerate);
	self->wrappedObject->set(TT("sampleRate"), samplerate);
	
	self->vs = maxvectorsize;
	
	self->audioIn->setVectorSizeWithInt(self->vs);
	self->audioOut->setVectorSizeWithInt(self->vs);
	
	object_method(dsp64, gensym("dsp_add64"), self, wrappedClass_perform64, 0, NULL);
}


TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c)
{
	return wrapTTClassAsMaxClass(ttblueClassName, maxClassName, c, (WrappedClassOptionsPtr)NULL);
}

TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, WrappedClassOptionsPtr options)
{
	TTObject		o(ttblueClassName, 1);	// Create a temporary instance of the class so that we can query it.
	TTValue			v;
	WrappedClass*	wrappedMaxClass = NULL;
	TTSymbol		name;
	TTCString		nameCString = NULL;
	t_symbol*		nameMaxSymbol = NULL;
	TTUInt32		nameSize = 0;
	
	common_symbols_init();
	TTDSPInit();
	
	if (!wrappedMaxClasses)
		wrappedMaxClasses = hashtab_new(0);
	
	wrappedMaxClass = new WrappedClass;
	wrappedMaxClass->maxClassName = gensym((char*)maxClassName);
	wrappedMaxClass->maxClass = class_new(	(char*)maxClassName, 
											(method)wrappedClass_new, 
											(method)wrappedClass_free, 
											sizeof(WrappedInstance), 
											(method)0L, 
											A_GIMME, 
											0);
	wrappedMaxClass->ttblueClassName = ttblueClassName;
	wrappedMaxClass->validityCheck = NULL;
	wrappedMaxClass->validityCheckArgument = NULL;
	wrappedMaxClass->options = options;
	wrappedMaxClass->maxNamesToTTNames = hashtab_new(0);
		
	if (!o.valid()) {
		error("Jamoma ClassWrapper failed to load %s", ttblueClassName.c_str());
		return kTTErrAllocFailed;
	}

	o.messages(v);
	for (TTUInt16 i=0; i<v.size(); i++) {
		name = v[i];
		//nameSize = name->getString().length();	// to -- this crash on Windows...
		nameSize = strlen(name.c_str());
		nameCString = new char[nameSize+1];
		strncpy_zero(nameCString, name.c_str(), nameSize+1);

		nameMaxSymbol = gensym(nameCString);
		hashtab_store(wrappedMaxClass->maxNamesToTTNames, nameMaxSymbol, (t_object*)name.rawpointer());
		class_addmethod(wrappedMaxClass->maxClass, (method)wrappedClass_anything, nameCString, A_GIMME, 0);
		
		delete nameCString;
		nameCString = NULL;
	}
	
	o.attributes(v);
	for (TTUInt16 i=0; i<v.size(); i++) {
		TTAttributePtr	attr = NULL;
		t_symbol*		maxType = _sym_long;
		
		name = v[i];
		//nameSize = name->getString().length();	// to -- this crash on Windows...
		nameSize = strlen(name.c_str());
		nameCString = new char[nameSize+1];
		strncpy_zero(nameCString, name.c_str(), nameSize+1);
		nameMaxSymbol = gensym(nameCString);
				
		if (name == TT("maxNumChannels"))
			continue;						// don't expose these attributes to Max users
		if (name == TT("bypass")) {
			if (wrappedMaxClass->options && !wrappedMaxClass->options->lookup(TT("generator"), v))
				continue;					// generators don't have inputs, and so don't really provide a bypass
		}
		
		o.instance()->findAttribute(name, &attr);
		
		if (attr->type == kTypeFloat32)
			maxType = _sym_float32;
		else if (attr->type == kTypeFloat64)
			maxType = _sym_float64;
		else if (attr->type == kTypeSymbol || attr->type == kTypeString)
			maxType = _sym_symbol;
		
		hashtab_store(wrappedMaxClass->maxNamesToTTNames, nameMaxSymbol, (t_object*)name.rawpointer());
		class_addattr(wrappedMaxClass->maxClass, attr_offset_new(nameCString, maxType, 0, (method)wrappedClass_attrGet, (method)wrappedClass_attrSet, 0));
		
		// Add display styles for the Max 5 inspector
		if (attr->type == kTypeBoolean)
			CLASS_ATTR_STYLE(wrappedMaxClass->maxClass, (char*)name.c_str(), 0, (char*)"onoff");
		if (name == TT("fontFace"))
			CLASS_ATTR_STYLE(wrappedMaxClass->maxClass,	(char*)"fontFace", 0, (char*)"font");
		
		delete nameCString;
		nameCString = NULL;
	}
			
 	class_addmethod(wrappedMaxClass->maxClass, (method)wrappedClass_dsp64, 		"dsp64",		A_CANT, 0L);
    class_addmethod(wrappedMaxClass->maxClass, (method)object_obex_dumpout, 	"dumpout",		A_CANT, 0); 
	class_addmethod(wrappedMaxClass->maxClass, (method)wrappedClass_assist, 	"assist",		A_CANT, 0L);
	class_addmethod(wrappedMaxClass->maxClass, (method)stdinletinfo,			"inletinfo",	A_CANT, 0);
	
	class_dspinit(wrappedMaxClass->maxClass);
	class_register(_sym_box, wrappedMaxClass->maxClass);
	if (c)
		*c = wrappedMaxClass;
	
	hashtab_store(wrappedMaxClasses, wrappedMaxClass->maxClassName, (t_object*)wrappedMaxClass);
	return kTTErrNone;
}


TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck)
{
	TTErr err = wrapTTClassAsMaxClass(ttblueClassName, maxClassName, c);
	
	if (!err) {
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = (*c)->maxClass;
	}
	return err;
}

TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, WrappedClassOptionsPtr options)
{
	TTErr err = wrapTTClassAsMaxClass(ttblueClassName, maxClassName, c, options);
	
	if (!err) {
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = (*c)->maxClass;
	}
	return err;
}


TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, TTPtr validityCheckArgument)
{
	TTErr err = wrapTTClassAsMaxClass(ttblueClassName, maxClassName, c);
	
	if (!err) {
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = validityCheckArgument;
	}
	return err;
}

TTErr wrapTTClassAsMaxClass(TTSymbol ttblueClassName, const char* maxClassName, WrappedClassPtr* c, TTValidityCheckFunction validityCheck, TTPtr validityCheckArgument, WrappedClassOptionsPtr options)
{
	TTErr err = wrapTTClassAsMaxClass(ttblueClassName, maxClassName, c, options);
	
	if (!err) {
		(*c)->validityCheck = validityCheck;
		(*c)->validityCheckArgument = validityCheckArgument;
	}
	return err;
}



TTErr TTValueFromAtoms(TTValue& v, long ac, t_atom* av)
{
	v.clear();
	
	// For now we assume floats for speed (e.g. in the performance sensitive j.unit object)
	for (int i=0; i<ac; i++)
		v.append((TTFloat64)atom_getfloat(av+i));
	return kTTErrNone;
}

TTErr TTAtomsFromValue(const TTValue& v, long* ac, t_atom** av)
{
	int	size = v.size();
	
	if (*ac && *av)
		; // memory was passed-in from the calling function -- use it
	else {
		*ac = size;
		*av = new t_atom[size];// (t_atom*)sysmem_newptr(sizeof(t_atom) * size);
	}

	for (int i=0; i<size; i++) {
		atom_setfloat((*av)+i, v[i]);
	}
	return kTTErrNone;
}


#include "jit.common.h"

/*
	We cannot count on Jitter matrices being tightly packed.
	There are alignment issues (rows are aligned to 16-byte boundaries)
	but there are additional issues, such as when accessing 1-plane of a 4-plane matrix, etc.
	Jitter is really expecting that any matrix will be broken down into vectors for processing.
	Jamoma does not make this assumption.
 
	Unfortunately, the only way to reliably get what we need into a tightly-packed Jamoma matrix
	is to do a copy -- and not just a copy but a row-by-row iterative copy.
 
	For these reasons, the 'copy' argument is optional, and it defaults to true.
 
	NOTE:	this function does not do the copying itself!
			this function references a matrix such that it is setup to serve as a copy.
 
	There is also the scenario where the output matrix from the Jamoma object is a different size
	than the input matrix, and the Jamoma object cannot change the size of a Jitter matrix.
*/

long TTMatrixReferenceJitterMatrix(TTMatrix aMatrix, TTPtr aJitterMatrix, TTBoolean copy)
{
	t_jit_matrix_info	jitterMatrixInfo;
	TTBytePtr			jitterMatrixData;
	long				jitterMatrixLock = (long)jit_object_method(aJitterMatrix, _sym_lock, 1);
	long				jitterDimensionCount;
	TTValue				dimensions;
	
	jit_object_method(aJitterMatrix, _sym_getinfo, &jitterMatrixInfo);
	jit_object_method(aJitterMatrix, _sym_getdata, &jitterMatrixData);
	
	if (!copy)
		aMatrix.referenceExternalData(jitterMatrixData);
	
	if (jitterMatrixInfo.type == _sym_char)
		aMatrix.set(kTTSym_type, kTTSym_uint8);
	else if (jitterMatrixInfo.type == _sym_long)
		aMatrix.set(kTTSym_type, kTTSym_int32);
	else if (jitterMatrixInfo.type == _sym_float32)
		aMatrix.set(kTTSym_type, kTTSym_float32);
	else if (jitterMatrixInfo.type == _sym_float64)
		aMatrix.set(kTTSym_type, kTTSym_float64);
	
	aMatrix.set(kTTSym_elementCount, (int)jitterMatrixInfo.planecount);
	
	jitterDimensionCount = jitterMatrixInfo.dimcount;
	dimensions.resize(jitterDimensionCount);
	
	for (int d=0; d < jitterDimensionCount; d++) {
		// The first 2 dimensions (rows and columns) are reversed in Jitter as compared to Jamoma
		if (d == 1)
			dimensions[0] = (int)jitterMatrixInfo.dim[d];
		else if (d==0 && jitterDimensionCount>1)
			dimensions[1] = (int)jitterMatrixInfo.dim[d];
		else
			dimensions[d] = (int)jitterMatrixInfo.dim[d];
	}
	
	aMatrix.set(kTTSym_dimensions, dimensions);
	
	return jitterMatrixLock;
}


// Assumes jitter matrix is locked, matrix dimensions agree , and we're ready to go 
TTErr TTMatrixCopyDataFromJitterMatrix(TTMatrix aMatrix, TTPtr aJitterMatrix)
{
	t_jit_matrix_info	jitterMatrixInfo;
	TTBytePtr			jitterMatrixData;
	TTValue				dimensions;
	int					dimcount;
	TTBytePtr			data = aMatrix.getLockedPointer();
	
	jit_object_method(aJitterMatrix, _sym_getinfo, &jitterMatrixInfo);
	jit_object_method(aJitterMatrix, _sym_getdata, &jitterMatrixData);
	
	dimcount = jitterMatrixInfo.dimcount;
	
	if (dimcount == 1) {
		memcpy(data, jitterMatrixData, jitterMatrixInfo.dimstride[0] * jitterMatrixInfo.dim[0]);
	}
	else if (dimcount == 2) {
		for (int i=0; i<jitterMatrixInfo.dim[1]; i++) { // step through the jitter matrix by row
			memcpy(data+(i*jitterMatrixInfo.dim[0] * aMatrix.getComponentStride()),
				   jitterMatrixData+(i*jitterMatrixInfo.dimstride[1]), 
				   jitterMatrixInfo.dimstride[0] * jitterMatrixInfo.dim[0]);
		}
	}
	else {
		// not supporting other dimcounts yet...
		aMatrix.releaseLockedPointer();
		return kTTErrInvalidType;
	}
	aMatrix.releaseLockedPointer();
	return kTTErrNone;
}


// Assumes jitter matrix is locked, matrix dimensions agree , and we're ready to go 
TTErr TTMatrixCopyDataToJitterMatrix(TTMatrix aMatrix, TTPtr aJitterMatrix)
{
	t_jit_matrix_info	jitterMatrixInfo;
	TTBytePtr			jitterMatrixData;
	TTValue				dimensions;
	int					dimcount;
	TTBytePtr			data = aMatrix.getLockedPointer();
	
	jit_object_method(aJitterMatrix, _sym_getinfo, &jitterMatrixInfo);
	jit_object_method(aJitterMatrix, _sym_getdata, &jitterMatrixData);
	
	dimcount = jitterMatrixInfo.dimcount;
	
	// TODO: need to actually resize the Jitter matrix?
	
	if (dimcount == 1) {
		memcpy(jitterMatrixData, data, jitterMatrixInfo.dimstride[0] * jitterMatrixInfo.dim[0]);
	}
	else if (dimcount == 2) {
		for (int i=0; i<jitterMatrixInfo.dim[1]; i++) {  // step through the jitter matrix by row
			memcpy(jitterMatrixData+(i*jitterMatrixInfo.dimstride[1]), 
				   data+(i*jitterMatrixInfo.dim[0] * aMatrix.getComponentStride()), 
				   jitterMatrixInfo.dimstride[0] * jitterMatrixInfo.dim[0]);
		}
	}
	else {
		// not supporting other dimcounts yet...
		aMatrix.releaseLockedPointer();
		return kTTErrInvalidType;
	}
	aMatrix.releaseLockedPointer();
	return kTTErrNone;
}
