#include <list>
#include <string>

#include "magickpp.hpp"

struct MagickPP {
	template <typename T> struct Container { typedef std::list<T> Type; };

	typedef char const* NameT;

	typedef Magick::CoderInfo CoderInfoT;
	typedef Magick::Image     ImageT;

	typedef Container<CoderInfoT>::Type CoderListT;
	typedef Container<ImageT>::Type     ImageListT;

	typedef ncbInstanceAdaptor<CoderInfoT>       CoderAdaptorT;
	typedef ncbInstanceAdaptor<ImageT>           ImageAdaptorT;


	// �C���[�W(�Q)�ǂݍ���
	static iTJSDispatch2* readImages(StringT const &spec) {

		ImageListT lst;
		Magick::readImages(&lst, spec);

		// Array �I�u�W�F�N�g���쐬
		iTJSDispatch2 *array = TJSCreateArrayObject();
		tjs_uint32 hint = 0;

		for (ImageListT::iterator it = lst.begin(); it != lst.end(); it++) {
			ImageT *img = new ImageT(*it);
			iTJSDispatch2 *adp = ImageAdaptorT::CreateAdaptor(img, false);
			tTJSVariant var(adp), *param = &var;
			adp->Release();
			array->FuncCall(0, TJS_W("add"), &hint, 0, 1, &param, array);
		}
		return array;
	}

	// Corder�ꗗ�擾
	static iTJSDispatch2* supports() {

		CoderListT cl; 
		Magick::coderInfoList(&cl,                   // Reference to output list 
							  CoderInfoT::AnyMatch,  // readable formats
							  CoderInfoT::AnyMatch,  // writable formats
							  CoderInfoT::AnyMatch); // multi-frame support

		// Array �I�u�W�F�N�g���쐬
		iTJSDispatch2 *array = TJSCreateArrayObject();
		tjs_uint32 hint = 0;

		ttstr exp(TJS_W("new MagickPP_CoderInfo('"));
		for (CoderListT::const_iterator it = cl.begin(); it != cl.end(); it++) {
			tTJSVariant var, *param = &var;
			TVPExecuteExpression(exp + ttstr(it->name().c_str()) + TJS_W("')"), &var);
			array->FuncCall(0, TJS_W("add"), &hint, 0, 1, &param, array);
		}

		return array;
	}

	// ImageMagick �o�[�W�����擾
	static NameT version() { return MagickVersion; }


};


NCB_REGISTER_CLASS(MagickPP) {
	PROP_RO(version);
	PROP_RO(supports);
}
