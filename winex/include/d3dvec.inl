#ifndef __WINE_D3DVEC_INL
#define __WINE_D3DVEC_INL

/*** constructors ***/

inline _D3DVECTOR::_D3DVECTOR(D3DVALUE f)
{
  x = y = z = f;
}

inline _D3DVECTOR::_D3DVECTOR(D3DVALUE _x, D3DVALUE _y, D3DVALUE _z)
{
  x = _x; y = _y; z = _z;
}

/*** assignment operators ***/

inline _D3DVECTOR& _D3DVECTOR::operator += (const _D3DVECTOR& v)
{
  x += v.x; y += v.y; z += v.z;
  return *this;
}

inline _D3DVECTOR& _D3DVECTOR::operator -= (const _D3DVECTOR& v)
{
  x -= v.x; y -= v.y; z -= v.z;
  return *this;
}

inline _D3DVECTOR& _D3DVECTOR::operator *= (const _D3DVECTOR& v)
{
  x *= v.x; y *= v.y; z *= v.z;
  return *this;
}

inline _D3DVECTOR& _D3DVECTOR::operator /= (const _D3DVECTOR& v)
{
  x /= v.x; y /= v.y; z /= v.z;
  return *this;
}

inline _D3DVECTOR& _D3DVECTOR::operator *= (D3DVALUE s)
{
  x *= s; y *= s; z *= s;
  return *this;
}

inline _D3DVECTOR& _D3DVECTOR::operator /= (D3DVALUE s)
{
  x /= s; y /= s; z /= s;
  return *this;
}

/*** unary operators ***/

inline _D3DVECTOR operator + (const _D3DVECTOR& v)
{
  return v;
}

inline _D3DVECTOR operator - (const _D3DVECTOR& v)
{
  return _D3DVECTOR(-v.x, -v.y, -v.z);
}

/*** binary operators ***/

inline _D3DVECTOR operator + (const _D3DVECTOR& v1, const _D3DVECTOR& v2)
{
  return _D3DVECTOR(v1.x+v2.x, v1.y+v2.y, v1.z+v2.z);
}

inline _D3DVECTOR operator - (const _D3DVECTOR& v1, const _D3DVECTOR& v2)
{
  return _D3DVECTOR(v1.x-v2.x, v1.y-v2.y, v1.z-v2.z);
}

inline _D3DVECTOR operator * (const _D3DVECTOR& v, D3DVALUE s)
{
  return _D3DVECTOR(v.x*s, v.y*s, v.z*s);
}

inline _D3DVECTOR operator * (D3DVALUE s, const _D3DVECTOR& v)
{
  return _D3DVECTOR(v.x*s, v.y*s, v.z*s);
}

inline _D3DVECTOR operator / (const _D3DVECTOR& v, D3DVALUE s)
{
  return _D3DVECTOR(v.x/s, v.y/s, v.z/s);
}

inline D3DVALUE SquareMagnitude(const _D3DVECTOR& v)
{
  return v.x*v.x + v.y*v.y + v.z*v.z; /* DotProduct(v, v) */
}

inline D3DVALUE Magnitude(const _D3DVECTOR& v)
{
  return sqrt(SquareMagnitude(v));
}

inline _D3DVECTOR Normalize(const _D3DVECTOR& v)
{
  return v / Magnitude(v);
}

inline D3DVALUE DotProduct(const _D3DVECTOR& v1, const _D3DVECTOR& v2)
{
  return v1.x*v2.x + v1.y*v2.y + v1.z*v2.z;
}

inline _D3DVECTOR CrossProduct(const _D3DVECTOR& v1, const _D3DVECTOR& v2)
{
  _D3DVECTOR res;
  /* this is a left-handed cross product, right? */
  res.x = v1.y * v2.z - v1.z * v2.y;
  res.y = v1.z * v2.x - v1.x * v2.z;
  res.z = v1.x * v2.y - v1.y * v2.x;
  return res;
}

inline _D3DMATRIX operator * (const _D3DMATRIX& m1, const _D3DMATRIX &m2)
{
  _D3DMATRIX res;
  res._11 = m1._11*m2._11 + m1._12*m2._21 + m1._13*m2._31 + m1._14*m2._41;
  res._12 = m1._11*m2._12 + m1._12*m2._22 + m1._13*m2._32 + m1._14*m2._42;
  res._13 = m1._11*m2._13 + m1._12*m2._23 + m1._13*m2._33 + m1._14*m2._43;
  res._14 = m1._11*m2._14 + m1._12*m2._24 + m1._13*m2._34 + m1._14*m2._44;
  res._21 = m1._21*m2._11 + m1._22*m2._21 + m1._23*m2._31 + m1._24*m2._41;
  res._22 = m1._21*m2._12 + m1._22*m2._22 + m1._23*m2._32 + m1._24*m2._42;
  res._23 = m1._21*m2._13 + m1._22*m2._23 + m1._23*m2._33 + m1._24*m2._43;
  res._24 = m1._21*m2._14 + m1._22*m2._24 + m1._23*m2._34 + m1._24*m2._44;
  res._31 = m1._31*m2._11 + m1._32*m2._21 + m1._33*m2._31 + m1._34*m2._41;
  res._32 = m1._31*m2._12 + m1._32*m2._22 + m1._33*m2._32 + m1._34*m2._42;
  res._33 = m1._31*m2._13 + m1._32*m2._23 + m1._33*m2._33 + m1._34*m2._43;
  res._34 = m1._31*m2._14 + m1._32*m2._24 + m1._33*m2._34 + m1._34*m2._44;
  res._41 = m1._41*m2._11 + m1._42*m2._21 + m1._43*m2._31 + m1._44*m2._41;
  res._42 = m1._41*m2._12 + m1._42*m2._22 + m1._43*m2._32 + m1._44*m2._42;
  res._43 = m1._41*m2._13 + m1._42*m2._23 + m1._43*m2._33 + m1._44*m2._43;
  res._44 = m1._41*m2._14 + m1._42*m2._24 + m1._43*m2._34 + m1._44*m2._44; 
  return res;
}

#endif
