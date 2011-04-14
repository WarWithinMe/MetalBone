// GObjBase.h
//////////////////////////////////////////////////////////////////////
#pragma once

namespace GObj
{

// The core template class for guarded operations.
// Incapsulates a wrapper around some type/object that needs cleanup.
template <class T>
class GBase_T {
protected:
	T m_Object;
	void DoDestroy()
	{
		if (IsValid())
			m_Object.Destroy();
	}
	void DoSetNull()
	{
		m_Object.m_Value = T::GetNullValue();
	}
public:
	// Destructor
	~GBase_T()
	{
		DoDestroy();
	}
	// Standard operations
	void Destroy()
	{
		if (IsValid())
		{
			m_Object.Destroy();
			DoSetNull();
		}
	}
	typename T::GuardType Detach()
	{
		typename T::GuardType val = m_Object.m_Value;
		DoSetNull();
		return val;
	}
	void Attach(typename T::GuardType val)
	{
		DoDestroy();
		m_Object.m_Value = val;
	}
	void Swap(GBase_T<T>& other)
	{
		swap_t(m_Object, other.m_Object);
	}
	// Extractors
	bool IsValid() const
	{
		return T::GetNullValue() != m_Object.m_Value;
	}
	bool IsNull() const
	{
		return T::GetNullValue() == m_Object.m_Value;
	}
	operator typename T::GuardType() const
	{
		return m_Object.m_Value;
	}
	operator bool () const
	{
		return IsValid();
	}
	// A more convenient way to access an object for some types
	typename T::GuardType operator ->()
	{
		return m_Object.m_Value;
	}
	typename T::GuardType GetValue() const
	{
		return m_Object.m_Value;
	}
	// The following method should be used with care!
	typename T::GuardType& GetRef()
	{
		return m_Object.m_Value;
	}
};

// A bit specialized version. Incapsulates objects that don't support referencing, only destruction.
// For such objects it is illegal to either assign one wrapper to another or use copy constructors,
// since in such a case two wrappers will attempt to destroy the object.
template <class T>
class GObj_T :public GBase_T<T> {
	// Disable copy constructor and assignment.
	GObj_T(const GObj_T& val);
	GObj_T& operator = (const GObj_T& val);
public:
	// Constructors
	GObj_T()
	{
		DoSetNull();
	}
	GObj_T(typename T::GuardType val)
	{
		m_Object.m_Value = val;
	}
	// Assignment
	GObj_T& operator = (typename T::GuardType val)
	{
		DoDestroy();
		m_Object.m_Value = val;
		return *this;
	}
};

// The following specialized version wraps objects that support referencing/dereferencing. Unlike
// previous case, here we can enable both copy constructors and all forms of the assignment.
template <class T>
class GRef_T :public GBase_T<T> {
protected:
	void DoAssign(typename T::GuardType val)
	{
		m_Object.m_Value = val;
		if (IsValid())
			m_Object.Reference();
	}
public:
	// Constructors
	GRef_T()
	{
		DoSetNull();
	}
	GRef_T(typename T::GuardType val)
	{
		DoAssign(val);
	}
	GRef_T(const GRef_T& val)
	{
		DoAssign(val);
	}
	GRef_T(typename T::GuardType val, bool bAddRef)
	{
		if (bAddRef)
			DoAssign(val);
		else
			m_Object.m_Value = val;
	}
	// Assignments
	GRef_T& operator = (typename T::GuardType val)
	{
		// This is needed if you intend to assign an object to itself.
		if (val != m_Object.m_Value)
		{
			DoDestroy();
			DoAssign(val);
		}
		return *this;
	}
	GRef_T& operator = (const GRef_T& val)
	{
		return operator = (val.m_Object.m_Value);
	}
};

// In order to use those guard tempate classes we've just defined - usually it's enough to
// typedef GObj_T/GRef_T<something>, but in some cases we'll also want to override those classes
// for some extra-functionality.
// Alas, the C++ compiler is stupid enough to generate copy constructor and assignment
// operator automatically, by just copying the mem. Of course this can't be a correct
// behaviour in our case. Because of this we've implemented those two functions explicitly
// in GObj_T/GRef_T classes.
// But unfortunately this is not enough to be absolutely sure the compiler won't ruine that.
// If you override a class and forget to re-write copy constructor/assignment the compiler
// will generate those two functions automatically again, incorrectly of course.
// To avoid such cases we obey the following rule: NEVER inherit the GObj_T class directly.
// Use the following macros instead:

#define INHERIT_GUARD_OBJ_BASE(gnew, gbase, gtype) \
private: \
	gnew(const gnew&); \
	gnew(const gbase&); \
	gnew& operator = (const gnew&); \
	gnew& operator = (const gbase&); \
public: \
	gnew& operator = (gtype val) { gbase::operator = (val); return *this; }

#define INHERIT_GUARD_OBJ(gnew, gbase, gtype) \
	INHERIT_GUARD_OBJ_BASE(gnew, gbase, gtype) \
	gnew() {} \
	gnew(gtype val) :gbase(val) {}

#define INHERIT_GUARD_REF_BASE(gnew, gbase, gtype) \
public: \
	gnew(const gnew& val) :gbase(val) {} \
	gnew(const gbase& val) :gbase(val) {} \
	gnew& operator = (gtype val) { gbase::operator = (val); return *this; } \
	gnew& operator = (const gnew& val) { gbase::operator = (val); return *this; } \
	gnew& operator = (const gbase& val) { gbase::operator = (val); return *this; }

#define INHERIT_GUARD_REF(gnew, gbase, gtype) \
	INHERIT_GUARD_REF_BASE(gnew, gbase, gtype) \
	gnew() {} \
	gnew(gtype val) :gbase(val) {} \
	gnew(gtype val, bool bAddRef) :gbase(val, bAddRef) {}

// Helper template classes, that are to be used in conjugtion with GObj_T/GRef_T classes.
// Core, carries the 'object' being guarded.
template <class T>
struct GBaseH_Core {
	T m_Value;
	typedef T GuardType;
};
// Implements GetNullValue by returning NULL of specified type, applicable for most of the cases.
template <class T>
struct GBaseH_Null {
	static T GetNullValue() { return NULL; }
};
// Implements GetNullValue by returning -1, applicable for some common types, such as files, sockets, and etc.
template <class T>
struct GBaseH_Minus1 {
	static T GetNullValue() { return (T) -1; }
};
// Mixes
template <class T>
struct GBaseH_CoreNull :public GBaseH_Core<T>, public GBaseH_Null<T> {
};
template <class T>
struct GBaseH_CoreMinus1 :public GBaseH_Core<T>, public GBaseH_Minus1<T> {
};

// Pointers to objects allocated via new operator.
template <class T>
struct GBaseH_Ptr :public GBaseH_CoreNull<T*> {
	void Destroy() { delete m_Value; }
};
template <class T>
class GPtr_T :public GObj_T<GBaseH_Ptr<T> >
{
	INHERIT_GUARD_OBJ(GPtr_T, GObj_T<GBaseH_Ptr<T> >, T*)
};

}; // namespace GObj
