#include "tjsinstance.h"
#include "tjsobj.h"

extern Local<Value> toJSValue(const tTJSVariant &variant);
extern tTJSVariant toVariant(Handle<Value> value);
extern tTJSVariant toVariant(Handle<Object> object, Handle<Object> context);

extern Handle<Value> ERROR_KRKR(tjs_error error);
extern Handle<Value> ERROR_BADINSTANCE();

extern Persistent<Context> mainContext;

/**
 * �g���g���ɑ΂��ė�O�ʒm
 */
void
JSEXCEPTION(TryCatch *try_catch)
{
	HandleScope handle_scope;
	String::Value exception(try_catch->Exception());

	Handle<Message> message = try_catch->Message();
	if (!message.IsEmpty()) {
		// ��O�\��
		String::Value filename(message->GetScriptResourceName());
		ttstr msg;
		msg += *filename;
		msg += ":";
		msg += tTJSVariant(message->GetLineNumber());
		msg += ":";
		msg += *exception;

		TVPAddLog(msg);
		
		// Print (filename):(line number): (message).
		String::Value sourceline(message->GetSourceLine());
		TVPAddLog(ttstr(*sourceline));

		// �G���[�s�\��
		ttstr wavy;
		int start = message->GetStartColumn();
		for (int i = 0; i < start; i++) {
			wavy += " ";
		}
		int end = message->GetEndColumn();
		for (int i = start; i < end; i++) {
			wavy +="^";
		}
		TVPAddLog(wavy);

		// �X�^�b�N�g���[�X�\��
		String::Value stack_trace(try_catch->StackTrace());
		if (stack_trace.length() > 0) {
			TVPAddLog(ttstr(*stack_trace));
		}
	}
	TVPThrowExceptionMessage(*exception);
}

/**
 * �����o�o�^�����p
 */
class MemberRegister : public tTJSDispatch /** EnumMembers �p */
{
public:
	// �R���X�g���N�^
	MemberRegister(Local<FunctionTemplate> &classTemplate) : classTemplate(classTemplate) {};
	
	// EnumMember�p�J��Ԃ����s��
	// param[0] �����o��
	// param[1] �t���O
	// param[2] �����o�̒l
	virtual tjs_error TJS_INTF_METHOD FuncCall( // function invocation
												tjs_uint32 flag,			// calling flag
												const tjs_char * membername,// member name ( NULL for a default member )
												tjs_uint32 *hint,			// hint for the member name (in/out)
												tTJSVariant *result,		// result
												tjs_int numparams,			// number of parameters
												tTJSVariant **param,		// parameters
												iTJSDispatch2 *objthis		// object as "this"
												) {
		if (numparams > 1) {
			if (param[2]->Type() == tvtObject) {
				const tjs_char *name = param[0]->GetString();
				tTVInteger flag = param[1]->AsInteger();
				bool staticMember = (flag & TJS_STATICMEMBER) != 0;
				iTJSDispatch2 *member = param[2]->AsObjectNoAddRef();
				if (member) {
					if (TJS_SUCCEEDED(member->IsInstanceOf(0,NULL,NULL,L"Function",NULL))) {
						registerFunction(name, *param[2], staticMember);
					} else if (TJS_SUCCEEDED(member->IsInstanceOf(0,NULL,NULL,L"Property",NULL))) {
						registerProperty(name, *param[2], staticMember);
					}
				}
			}
		}
		if (result) {
			*result = true;
		}
		return TJS_S_OK;
	}

private:
	// �t�@���N�V�����o�^
	void registerFunction(const tjs_char *functionName, tTJSVariant &function, bool staticMember) {
		classTemplate->PrototypeTemplate()->Set(String::New(functionName), FunctionTemplate::New(TJSInstance::tjsInvoker, TJSObject::toJSObject(function)));
	}
	
	// �v���p�e�B�o�^
	void registerProperty(const tjs_char *propertyName, tTJSVariant &property, bool staticMember) {
		classTemplate->PrototypeTemplate()->SetAccessor(String::New(propertyName), TJSInstance::tjsGetter, TJSInstance::tjsSetter, TJSObject::toJSObject(property));
	}
	
	Local<FunctionTemplate> &classTemplate;
};

// -----------------------------------------------------------------------

int TJSInstance::classId;

// �������p
void
TJSInstance::init(Handle<ObjectTemplate> globalTemplate)
{
	// �l�C�e�B�u�C���X�^���X�o�^�p�N���XID�L�^
	classId = TJSRegisterNativeClass(L"JavascriptClass");
	// ���\�b�h��o�^
	globalTemplate->Set(String::New("createTJSClass"), FunctionTemplate::New(createTJSClass));
}

/**
 * �g���g���N���X���� Javascript �N���X�𐶐�
 * @param args ����
 * @return ����
 */
Handle<Value>
TJSInstance::createTJSClass(const Arguments& args)
{
	if (args.Length() < 1) {
		return ThrowException(String::New("invalid param"));
	}

	// TJS�N���X���擾
	String::Value tjsClassName(args[0]);
	tTJSVariant tjsClassObj;
	TVPExecuteExpression(*tjsClassName, &tjsClassObj);
	if (tjsClassObj.Type() != tvtObject || TJS_FAILED(tjsClassObj.AsObjectClosureNoAddRef().IsInstanceOf(0,NULL,NULL,L"Class",NULL))) {
		ThrowException(String::New("invalid param"));
	}
	
	// �N���X�e���v���[�g�𐶐�
	Local<FunctionTemplate> classTemplate = FunctionTemplate::New(tjsConstructor, TJSObject::toJSObject(tjsClassObj));
	classTemplate->SetClassName(args[0]->ToString()); // �\����
	
	// �����o�o�^����
	for (int i=args.Length()-1;i>=0;i--) {
		String::Value className(args[i]);
		tTJSVariant classObj;
		TVPExecuteExpression(*className, &classObj);
		if (classObj.Type() == tvtObject &&
			TJS_SUCCEEDED(classObj.AsObjectClosureNoAddRef().IsInstanceOf(0,NULL,NULL,L"Class",NULL))) {
			MemberRegister *r = new MemberRegister(classTemplate);
			tTJSVariantClosure closure(r);
			classObj.AsObjectClosureNoAddRef().EnumMembers(TJS_IGNOREPROP, &closure, NULL);
			r->Release();
		}
	}

	// TJS�@�\���\�b�h��o�^
	Local<ObjectTemplate> protoTemplate = classTemplate->PrototypeTemplate();
	protoTemplate->Set(String::New("tjsIsValid"), FunctionTemplate::New(tjsIsValid));
	protoTemplate->Set(String::New("tjsOverride"), FunctionTemplate::New(tjsOverride));
	
	return classTemplate->GetFunction();
}

/**
 * �g���g���I�u�W�F�N�g�� javascript�I�u�W�F�N�g�ɕϊ�
 * @return �o�^����
 */
bool
TJSInstance::getJSObject(Local<Object> &result, const tTJSVariant &variant)
{
	iTJSDispatch2 *dispatch = variant.AsObjectNoAddRef();
	iTJSNativeInstance *ninstance;
	if (TJS_SUCCEEDED(dispatch->NativeInstanceSupport(TJS_NIS_GETINSTANCE, classId, &ninstance))) {
		// Javascript������o�^���ꂽ�I�u�W�F�N�g�̏ꍇ�͌��� Javascript�I�u�W�F�N�g�������̂܂ܕԂ�
		TJSInstance *self = (TJSInstance*)ninstance;
		result = *(self->self);
		return true;
	}
	return false;
}

// �v���p�e�B�擾���ʏ���
tjs_error
TJSInstance::getProp(Local<Object> obj, const tjs_char *membername, tTJSVariant *result)
{
	if (!membername) {
		return TJS_E_NOTIMPL;
	}
	
	HandleScope handle_scope;
	Context::Scope context_scope(mainContext);
	TryCatch try_catch;
	
	Local<Value> ret = obj->Get(String::New(membername));
	if (ret.IsEmpty()) {
		return TJS_E_MEMBERNOTFOUND;
	} else {
		if (result) {
			if (ret->IsFunction()) {
				*result = toVariant(ret->ToObject(), obj);
			} else {
				*result = toVariant(ret);
			}
		}
	}
	return TJS_S_OK;
}

// �v���p�e�B�ݒ苤�ʏ���
tjs_error
TJSInstance::setProp(Local<Object> obj, const tjs_char *membername, const tTJSVariant *param)
{
	if (!membername) {
		return TJS_E_NOTIMPL;
	}
	
	HandleScope handle_scope;
	Context::Scope context_scope(mainContext);
	TryCatch try_catch;
	
	if (obj->Set(String::New(membername), toJSValue(*param))) {
		return TJS_S_OK;
	}
	return TJS_E_MEMBERNOTFOUND;
}

tjs_error
TJSInstance::remove(Local<Object> obj, const tjs_char *membername)
{
	if (!membername) {
		return TJS_E_NOTIMPL;
	}
	HandleScope handle_scope;
	Context::Scope context_scope(mainContext);
	TryCatch try_catch;
	return obj->Delete(String::New(membername)) ? TJS_S_OK : TJS_S_FALSE;
}

// �R���X�g���N�^�Ăяo�����ʏ���
tjs_error
TJSInstance::createMethod(Local<Object> obj, const tjs_char *membername, iTJSDispatch2 **result, tjs_int numparams, tTJSVariant **param)
{
	if (membername) {
		return TJS_E_MEMBERNOTFOUND;
	}

	HandleScope handle_scope;
	Context::Scope context_scope(mainContext);
	TryCatch try_catch;

	if (!obj->IsFunction()) {
		return TJS_E_NOTIMPL;
	}
	
	// �֐����o
	Local<Function> func = Local<Function>::Cast(obj->ToObject());
	// ����
	Handle<Value> *argv = new Handle<Value>[numparams];
	for (int i=0;i<numparams;i++) {
		argv[i] = toJSValue(*param[i]);
	}
	Handle<Object> ret = func->NewInstance(numparams, argv);
	delete argv;
	
	if (ret.IsEmpty()) {
		JSEXCEPTION(&try_catch);
	} else {
		if (result) {
			*result = toVariant(ret);
		}
	}
	return TJS_S_OK;
}

// ���\�b�h�Ăяo�����ʏ���
tjs_error
TJSInstance::callMethod(Local<Object> obj, const tjs_char *membername, tTJSVariant *result, tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *objthis)
{
	HandleScope handle_scope;
	Context::Scope context_scope(mainContext);
	TryCatch try_catch;

	Local<Object> context = membername ? obj : objthis ? toJSValue(tTJSVariant(objthis))->ToObject() : mainContext->Global();
	Local<Object> method  = membername ? obj->Get(String::New(membername))->ToObject() : obj;

	if (!method->IsFunction()) {
		return TJS_E_NOTIMPL;
	}
	
	// �֐����o
	Local<Function> func = Local<Function>::Cast(method);
	// ����
	Handle<Value> *argv = new Handle<Value>[numparams];
	for (int i=0;i<numparams;i++) {
		argv[i] = toJSValue(*param[i]);
	}
	Handle<Value> ret = func->Call(context, numparams, argv);
	delete argv;
	
	if (ret.IsEmpty()) {
		JSEXCEPTION(&try_catch);
	} else {
		if (result) {
			*result = toVariant(ret);
		}
	}
	return TJS_S_OK;
}

// -----------------------------------------------------------------------


//---------------------------------------------------------------------------
// missing�֐�
//---------------------------------------------------------------------------
class tMissingFunction : public tTJSDispatch
{
	tjs_error TJS_INTF_METHOD FuncCall(tjs_uint32 flag, const tjs_char * membername, tjs_uint32 *hint,
									   tTJSVariant *result,
									   tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *objthis) {
		return TJSInstance::missing(flag, membername, hint, result, numparams, param, objthis);
	};
};

/**
 * missing �����p�̌�
 * TJS�C���X�^���X�Ƀ����o�����݂��Ȃ������ꍇ�� javascript�C���X�^���X���Q�Ƃ���
 */
tjs_error TJSInstance::missing(tjs_uint32 flag, const tjs_char * membername, tjs_uint32 *hint,
							 tTJSVariant *result,
							 tjs_int numparams, tTJSVariant **params, iTJSDispatch2 *objthis) {
	
	if (numparams < 3) {return TJS_E_BADPARAMCOUNT;};
	iTJSNativeInstance *ninstance;
	if (TJS_SUCCEEDED(objthis->NativeInstanceSupport(TJS_NIS_GETINSTANCE, classId, &ninstance))) {
		TJSInstance *self = (TJSInstance*)ninstance;
		bool ret = false;
		if (!(int)*params[0]) { // get
			tTJSVariant result;
			if (TJS_SUCCEEDED(getProp(*(self->self), params[1]->GetString(), &result))) {
				params[2]->AsObjectClosureNoAddRef().PropSet(0, NULL, NULL, &result, NULL);
				ret = true;
			}
		} else { // set
			if (TJS_SUCCEEDED(setProp(*(self->self), params[1]->GetString(), params[2]))) {
				ret = true;
			}
		}
		if (result) {
			*result = ret;
		}
	}
	return TJS_E_NATIVECLASSCRASH;
}

//---------------------------------------------------------------------------
// callJS�֐�
//---------------------------------------------------------------------------
class tCallJSFunction : public tTJSDispatch
{
	tjs_error TJS_INTF_METHOD FuncCall(tjs_uint32 flag, const tjs_char * membername, tjs_uint32 *hint,
									   tTJSVariant *result,
									   tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *objthis) {
		return TJSInstance::call(flag, membername, hint, result, numparams, param, objthis);
	};
};

/**
 * call �����p�̌�
 * TJS�C���X�^���X����javascript�C���X�^���X�̃��\�b�h�𒼐ڌĂяo��
 */
tjs_error
TJSInstance::call(tjs_uint32 flag, const tjs_char * membername, tjs_uint32 *hint,
				tTJSVariant *result,
				tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *objthis)
{
	if (numparams < 1) {return TJS_E_BADPARAMCOUNT;};
	iTJSNativeInstance *ninstance;
	if (TJS_SUCCEEDED(objthis->NativeInstanceSupport(TJS_NIS_GETINSTANCE, classId, &ninstance))) {
		TJSInstance *self = (TJSInstance*)ninstance;
		return callMethod(self->self->ToObject(), param[0]->GetString(), result, numparams-1, param+1, objthis);
	}
	return TJS_E_NATIVECLASSCRASH;
}


/**
 * �R���X�g���N�^
 */
TJSInstance::TJSInstance(Handle<Object> obj, const tTJSVariant &variant) : TJSBase(variant)
{
	// Javascript �I�u�W�F�N�g�Ɋi�[
	wrap(obj);
	self = Persistent<Object>::New(obj);
	self.MakeWeak(this, release);
	
	iTJSDispatch2 *objthis = variant.AsObjectNoAddRef();

	// TJS�C���X�^���X�Ƀl�C�e�B�u�C���X�^���X�Ƃ��ēo�^���Ă���
	iTJSNativeInstance *ninstance = this;
	objthis->NativeInstanceSupport(TJS_NIS_REGISTER, classId, &ninstance);

	// callJS ���\�b�h�o�^
	tCallJSFunction *callJS = new tCallJSFunction();
	if (callJS) {
		tTJSVariant val(callJS, objthis);
		objthis->PropSet(TJS_MEMBERENSURE, TJS_W("callJS"), NULL, &val, objthis);
		callJS->Release();
	}
	
	// missing ���\�b�h�o�^
	tMissingFunction *missing = new tMissingFunction();
	if (missing) {
		tTJSVariant val(missing, objthis);
		const tjs_char *missingName = TJS_W("missing");
		objthis->PropSet(TJS_MEMBERENSURE, missingName, NULL, &val, objthis);
		missing->Release();
		// missing �L����
		tTJSVariant name(missingName);
		objthis->ClassInstanceInfo(TJS_CII_SET_MISSING, 0, &name);
	}
}

static bool TJS_USERENTRY Catch(void * data, const tTVPExceptionDesc & desc) {
	return false;
}

static void TJS_USERENTRY TryInvalidate(void * data) {
	tTJSVariant *v = (tTJSVariant*)data;
	v->AsObjectClosureNoAddRef().Invalidate(0, NULL, NULL, NULL);
}

void
TJSInstance::invalidate()
{
	// TJS�I�u�W�F�N�g��j�󂵂ĎQ�Ƃ��N���A����
	// ����ɂ��ATJS���ŉ�����������肱�̃I�u�W�F�N�g���̂�
	// �l�C�e�B�u�C���X�^���X�̃N���A�����Ŕj�������
	if (variant.Type() == tvtObject && variant.AsObjectClosureNoAddRef().IsValid(0, NULL, NULL, NULL) == TJS_S_TRUE) {
		TVPDoTryBlock(TryInvalidate, Catch, NULL, (void *)&variant);
	}
	variant.Clear();
}

/**
 * �I�u�W�F�N�g�̃����[�T
 */
void
TJSInstance::release(Persistent<Value> handle, void* parameter)
{
	TJSInstance *self = (TJSInstance*)parameter;
	if (self) {
		self->invalidate();
	}
}

// ---------------------------
// NativeInstance �Ή��p�����o
// ---------------------------

// �������ĂѕԂ�
tjs_error TJS_INTF_METHOD
TJSInstance::Construct(tjs_int numparams, tTJSVariant **param, iTJSDispatch2 *tjs_obj)
{
	return TJS_S_OK;
}

// Invalidate���ĂѕԂ�
void TJS_INTF_METHOD
TJSInstance::Invalidate()
{
}

// �j�����ĂѕԂ�
void TJS_INTF_METHOD
TJSInstance::Destruct()
{
	delete this;
}

// -----------------------------------------------------------------------------------------------------------

// TJS�̗�O����Ăяo�������p
class CreateInfo {

public:
	// �R���X�g���N�^
	CreateInfo(const tTJSVariant &classObj, const Arguments& args) : classObj(classObj), args(args), argc(0), argv(NULL) {
		// ��������
		argc = args.Length();
		if (argc > 0) {
			argv = new tTJSVariant*[(size_t)argc];
			for (tjs_int i=0;i<argc;i++) {
				argv[i] = new tTJSVariant();
				*argv[i] = toVariant(args[i]);
			}
		}
	}

	// �f�X�g���N�^
	~CreateInfo() {
		// �����j��
		if (argv) {
			for (int i=0;i<argc;i++) {
				delete argv[i];
			}
			delete[] argv;
		}
	}
	
	Handle<Value> create() {
		TVPDoTryBlock(TryCreate, Catch, Finally, (void *)this);
		return ret;
	}

private:

	void _TryCreate() {
		tjs_error error;
		iTJSDispatch2 *newinstance;
		if (TJS_SUCCEEDED(error = classObj.AsObjectClosureNoAddRef().CreateNew(0, NULL, NULL, &newinstance, argc, argv, NULL))) {
			new TJSInstance(args.This(), tTJSVariant(newinstance, newinstance));
			newinstance->Release();
			ret = args.This();
		} else {
			ret = ERROR_KRKR(error);
		}
	}

	static void TJS_USERENTRY TryCreate(void * data) {
		CreateInfo *info = (CreateInfo*)data;
		info->_TryCreate();
	}

	static bool TJS_USERENTRY Catch(void * data, const tTVPExceptionDesc & desc) {
		CreateInfo *info = (CreateInfo*)data;
		info->ret = ThrowException(String::New(desc.message.c_str()));
		// ��O�͏�ɖ���
		return false;
	}

	static void TJS_USERENTRY Finally(void * data) {
	}

private:
	const tTJSVariant &classObj;
	const Arguments& args;
	tjs_int argc;
	tTJSVariant **argv;
	Handle<Value> ret;
};

// TJS�̗�O����Ăяo�������p
class FuncInfo {

public:
	// �R���X�g���N�^
	FuncInfo(const tTJSVariant &instance, const tTJSVariant &method, const Arguments& args) : instance(instance), method(method), argc(0), argv(NULL) {
		// ��������
		argc = args.Length();
		if (argc > 0) {
			argv = new tTJSVariant*[(size_t)argc];
			for (tjs_int i=0;i<argc;i++) {
				argv[i] = new tTJSVariant();
				*argv[i] = toVariant(args[i]);
			}
		}
	}

	// �f�X�g���N�^
	~FuncInfo() {
		// �����j��
		if (argv) {
			for (int i=0;i<argc;i++) {
				delete argv[i];
			}
			delete[] argv;
		}
	}
	
	Handle<Value> exec() {
		TVPDoTryBlock(TryExec, Catch, Finally, (void *)this);
		return ret;
	}

private:

	void _TryExec() {
		tjs_error error;
		tTJSVariant r;
		if (TJS_SUCCEEDED(error = method.AsObjectNoAddRef()->FuncCall(0, NULL, NULL, &r, argc, argv, instance.AsObjectNoAddRef()))) {
			result = toJSValue(r);
			ret = result;
		} else {
			ret = ERROR_KRKR(error);
		}
	}
	
	static void TJS_USERENTRY TryExec(void * data) {
		FuncInfo *info = (FuncInfo*)data;
		info->_TryExec();
	}

	static bool TJS_USERENTRY Catch(void * data, const tTVPExceptionDesc & desc) {
		FuncInfo *info = (FuncInfo*)data;
		info->ret = ThrowException(String::New(desc.message.c_str()));
		// ��O�͏�ɖ���
		return false;
	}

	static void TJS_USERENTRY Finally(void * data) {
	}

private:
	const tTJSVariant &instance;
	const tTJSVariant &method;
	tjs_int argc;
	tTJSVariant **argv;
	Local<Value> result;
	Handle<Value> ret;
};

// TJS�̗�O����Ăяo�������p
class PropSetter {

public:
	// �R���X�g���N�^
	PropSetter(const tTJSVariant &instance, const tTJSVariant &method, Local<Value> value, const AccessorInfo& info) : instance(instance), method(method) {
		param    = toVariant(value);
	}
	
	void exec() {
		TVPDoTryBlock(TrySetter, Catch, Finally, (void *)this);
	}

private:
	void _TrySetter() {
		method.AsObjectNoAddRef()->PropSet(TJS_MEMBERENSURE, NULL, NULL, &param, instance.AsObjectNoAddRef());
	}
	
	static void TJS_USERENTRY TrySetter(void * data) {
		PropSetter *info = (PropSetter*)data;
		info->_TrySetter();
	}
	
	static bool TJS_USERENTRY Catch(void * data, const tTVPExceptionDesc & desc) {
		// ��O�͏�ɖ���
		return false;
	}
	
	static void TJS_USERENTRY Finally(void * data) {
	}

private:
	const tTJSVariant &instance;
	const tTJSVariant &method;
	tTJSVariant param;
};

// TJS�̗�O����Ăяo�������p
class PropGetter {

public:
	// �R���X�g���N�^
	PropGetter(const tTJSVariant &instance, const tTJSVariant &method) : instance(instance), method(method) {
	}
	
	Handle<Value> exec() {
		TVPDoTryBlock(TryGetter, Catch, Finally, (void *)this);
		return ret;
	}

private:
	void _TryGetter() {
		tjs_error error;
		tTJSVariant r;
		if (TJS_SUCCEEDED(error = method.AsObjectNoAddRef()->PropGet(0, NULL, NULL, &r, instance.AsObjectNoAddRef()))) {
			result = toJSValue(r);
			ret = result;
		} else {
			ret = ERROR_KRKR(error);
		}
	}
	
	static void TJS_USERENTRY TryGetter(void * data) {
		PropGetter *info = (PropGetter*)data;
		info->_TryGetter();
	}

	static bool TJS_USERENTRY Catch(void * data, const tTVPExceptionDesc & desc) {
		PropGetter *info = (PropGetter*)data;
		info->ret = ThrowException(String::New(desc.message.c_str()));
		// ��O�͏�ɖ���
		return false;
	}
	
	static void TJS_USERENTRY Finally(void * data) {
	}

private:
	const tTJSVariant &instance;
	const tTJSVariant &method;
	Local<Value> result;
	Handle<Value> ret;
};

/**
 * TJS�I�u�W�F�N�g�̃R���X�g���N�^
 */
Handle<Value>
TJSInstance::tjsConstructor(const Arguments& args)
{
	tTJSVariant classObj;
	if (getVariant(classObj, args.Data()->ToObject())) {
		CreateInfo info(classObj, args);
		return info.create();
	}
	return ERROR_BADINSTANCE();
}

/**
 * TJS�I�u�W�F�N�g�p�̃��\�b�h
 * @param args ����
 * @return ����
 */
Handle<Value>
TJSInstance::tjsInvoker(const Arguments& args)
{
	tTJSVariant instance;
	tTJSVariant method;
	if (getVariant(instance, args.This()) && getVariant(method, args.Data()->ToObject())) {
		FuncInfo info(instance, method, args);
		return info.exec();
	}
	return ERROR_BADINSTANCE();
}

/**
 * TJS�I�u�W�F�N�g�p�̃v���p�e�B�Q�b�^�[
 * @param args ����
 * @return ����
 */
Handle<Value>
TJSInstance::tjsGetter(Local<String> property, const AccessorInfo& info)
{
	tTJSVariant instance;
	tTJSVariant method;
	if (getVariant(instance, info.This()) && getVariant(method, info.Data()->ToObject())) {
		PropGetter get(instance, method);
		return get.exec();
	}
	return ERROR_BADINSTANCE();
}

/**
 * TJS�I�u�W�F�N�g�p�̃v���p�e�B�Z�b�^�[
 * @param args ����
 * @return ����
 */
void
TJSInstance::tjsSetter(Local<String> property, Local<Value> value, const AccessorInfo& info)
{
	tTJSVariant instance;
	tTJSVariant method;
	if (getVariant(instance, info.This()) && getVariant(method, info.Data()->ToObject())) {
		PropSetter set(instance, method, value, info);
		set.exec();
	}
}

/**
 * TJS�I�u�W�F�N�g�̗L���m�F
 * @param args ����
 * @return ����
 */
Handle<Value>
TJSInstance::tjsIsValid(const Arguments& args)
{
	tTJSVariant instance;
	if (getVariant(instance, args.This())) {
		return Boolean::New(instance.AsObjectClosureNoAddRef().IsValid(0, NULL, NULL, NULL) == TJS_S_TRUE);
	}
	return ERROR_BADINSTANCE();
}

/**
 * TJS�I�u�W�F�N�g�̃I�[�o���C�h����
 * @param args ����
 * @return ����
 */
Handle<Value>
TJSInstance::tjsOverride(const Arguments& args)
{
	tTJSVariant instance;
	if (getVariant(instance, args.This())) {
		if (args.Length() > 0) {
			Handle<Value> func = args.Length() > 1 ? args[1] : args.This()->Get(args[0]);
			if (func->IsFunction()) {
				tTJSVariant value = toVariant(func->ToObject(), args.This());
				String::Value methodName(args[0]);
				tjs_error error;
				if (TJS_FAILED(error = instance.AsObjectClosureNoAddRef().PropSet(TJS_MEMBERENSURE, *methodName, NULL, &value, NULL))) {
					return ERROR_KRKR(error);
				}
				return Undefined();
			}
		}
		return ThrowException(String::New("not function"));
	}
	return ERROR_BADINSTANCE();
}