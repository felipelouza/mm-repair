/* >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
 * Basic definitions for 1-dimensional dense vectors
 * >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> */
// one dimensional dense uncompressed vector 
typedef struct {
  matval *v;
  size_t size;
} vector;


vector *vector_create();
xmatval vector_normalize(vector *v);
void vector_destroy(vector *v);


// ---------- vectors -----------------

// return a pointer to an empty vector object
vector *vector_create()
{
  vector *w = malloc(sizeof(vector));
  if(w==NULL) die("Realloc failed");
  w->v = NULL;
  w->size=0;
  return w;
}


// infinity norm normalization, return the norm before normalization
xmatval vector_normalize(vector *w)
{
  xmatval norm = 0;
  for(int i=0;i<w->size;i++) {
    xmatval t = w->v[i]>0 ? w->v[i] : -w->v[i];
    if(t>norm) norm=t;
  }
  assert(norm>=0);
  if(norm>0) 
    for(int i=0;i<w->size;i++)
      w->v[i] = w->v[i]/norm;
  return norm;
}  

// set v = v + t w
void vector_update(vector *v, xmatval t, vector *w)
{
  assert(v->v!=NULL && w->v!=NULL);
  assert(v->size==w->size);
  for(int i=0;i<v->size;i++)
    v->v[i] += t*w->v[i];
}

// set v = tv + tw
void vector_scalar_update(vector *v, xmatval t)
{
  assert(v->v!=NULL);
  for(int i=0;i<v->size;i++)
    v->v[i] *= t;
}

// norm2 squared computation
xmatval vector_norm2sq(vector *w)
{
  xmatval norm = 0;
  for(int i=0;i<w->size;i++) {
    norm +=   w->v[i] * w->v[i];
  }
  return norm;  
}

// scalar product of two vectors 
xmatval vector_scalar_prod(vector *v, vector *w)
{
  assert(v->size==w->size);
  xmatval sp = 0;
  for(int i=0;i<w->size;i++) {
    sp +=   w->v[i] * w->v[i];
  }
  return sp;  
}
// set an existing vector to 0
void vector_set_zero(vector *v, int dim)
{
  v->size = dim;
  v->v = realloc(v->v,dim*sizeof(matval));
  if(v->v==NULL) die("Realloc failed");
  for(int i=0;i<v->size;i++) v->v[i]=0;  
}

void vector_destroy(vector *v)
{
  if(v->v!=NULL) free(v->v);
  free(v);
}


