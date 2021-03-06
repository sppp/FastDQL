#include "FastDQL.h"
#include <random>

namespace FastDQL {


class RandomGaussian {
	std::default_random_engine generator;
	std::normal_distribution<double> distribution;
	
public:

	// weight normalization is done to equalize the output
	// variance of every neuron, otherwise neurons with a lot
	// of incoming connections have outputs of larger variance
	RandomGaussian(int length) : distribution(0, sqrt(1.0 / (double)(length))) {
		generator.seed(Random(1000000000));
	}
	double Get() {return distribution(generator);}
	operator double() {return distribution(generator);}
	
};

template <class T> // Workaround for GCC bug - specialization needed...
T& SingleRandomGaussianLock() {
	static T o;
	return o;
}

inline RandomGaussian& GetRandomGaussian(int length) {
	SpinLock& lock = SingleRandomGaussianLock<SpinLock>(); // workaround
	ArrayMap<int, RandomGaussian>& rands = Single<ArrayMap<int, RandomGaussian> >();
	lock.Enter();
	int i = rands.Find(length);
	RandomGaussian* r;
	if (i == -1) {
		r = &rands.Add(length, new RandomGaussian(length));
	} else {
		r = &rands[i];
	}
	lock.Leave();
	return *r;
}





Mat::Mat() {
	width = 0;
	height = 0;
	length = 0;
}

Mat::Mat(int width, int height) {
	ASSERT(width > 0 && height > 0);
	Init(width, height);
}

Mat::Mat(int width, int height, double c) {
	ASSERT(width > 0 && height > 0);
	Init(width, height, c);
}

Mat::Mat(const Vector<double>& weights) {
	// we were given a list in weights, assume 1D volume and fill it up
	width = 1;
	height = weights.GetCount();
	length = height;
	
	this->weights <<= weights;
	
	weight_gradients.SetCount(length, 0.0);
}

Mat::Mat(int width, int height, const Vector<double>& weights) {
	ASSERT(width > 0 && height > 0);
	this->width = width;
	this->height = height;
	length = width * height;
	
	ASSERT(length == weights.GetCount());
	
	this->weights <<= weights;
	
	weight_gradients.SetCount(length, 0.0);
}

Mat::Mat(int width, int height, Mat& vol) {
	ASSERT(width > 0 && height > 0);
	this->width = width;
	this->height = height;
	length = width * height;
	
	this->weights <<= vol.weights;
	
	ASSERT(this->weights.GetCount() == length);
	
	weight_gradients.SetCount(length, 0.0);
}

Mat::~Mat() {
	
}

int Mat::GetMaxColumn() const {
	double max = -DBL_MAX;
	int pos = -1;
	for(int i = 0; i < weights.GetCount(); i++) {
		double d = weights[i];
		if (i == 0 || d > max) {
			max = d;
			pos = i;
		}
	}
	return pos;
}

int Mat::GetSampledColumn() const {
	// sample argmax from w, assuming w are
	// probabilities that sum to one
	double r = Randomf();
	double x = 0.0;
	for(int i = 0; i < weights.GetCount(); i++) {
		x += weights[i];
		if (x > r) {
			return i;
		}
	}
	return weights.GetCount() - 1; // pretty sure we should never get here?
}

Mat& Mat::operator=(const Mat& src) {
	width = src.width;
	height = src.height;
	length = src.length;
	weights.SetCount(src.weights.GetCount());
	for(int i = 0; i < weights.GetCount(); i++)
		weights.Set(i, src.weights[i]);
	weight_gradients.SetCount(src.weight_gradients.GetCount());
	for(int i = 0; i < weight_gradients.GetCount(); i++)
		weight_gradients[i] = src.weight_gradients[i];
	return *this;
}

Mat& Mat::Init(int width, int height) {
	ASSERT(width > 0 && height > 0);
	
	// we were given dimensions of the vol
	this->width = width;
	this->height = height;
	
	int n = width * height;
	
	length = n;
	weights.SetCount(n, 0.0);
	weight_gradients.SetCount(n, 0.0);
	
	RandomGaussian& rand = GetRandomGaussian(length);

	for (int i = 0; i < n; i++) {
		weights.Set(i, rand);
	}
	
	return *this;
}


Mat& Mat::Init(int width, int height, double default_value) {
	ASSERT(width > 0 && height > 0);
	
	// we were given dimensions of the vol
	this->width = width;
	this->height = height;
	
	int n = width * height;
	int prev_length = length;
	
	length = n;
	weights.SetCount(n);
	weight_gradients.SetCount(n);
	
	for (int i = 0; i < n; i++) {
		weights.Set(i, default_value);
		weight_gradients[i] = 0.0;
	}
	
	return *this;
}

Mat& Mat::Init(int width, int height, const Vector<double>& w) {
	ASSERT(width > 0 && height > 0);
	
	// we were given dimensions of the vol
	this->width = width;
	this->height = height;
	
	int n = width * height;
	ASSERT(n == w.GetCount());
	
	length = n;
	weights.SetCount(n);
	weight_gradients.SetCount(n);
	
	for (int i = 0; i < n; i++) {
		weights[i] = w[i];
		weight_gradients[i] = 0.0;
	}
	
	return *this;
}

int Mat::GetPos(int x, int y) const {
	ASSERT(x >= 0 && y >= 0 && x < width && y < height);
	return (width * y) + x;
}

double Mat::Get(int x, int y) const {
	int ix = GetPos(x,y);
	return weights[ix];
}

void Mat::Set(int x, int y, double v) {
	int ix = GetPos(x,y);
	weights[ix] = v;
}

void Mat::Add(int x, int y, double v) {
	int ix = GetPos(x,y);
	weights[ix] += v;
}

void Mat::Add(int i, double v) {
	weights[i] += v;
}

double Mat::GetGradient(int x, int y) const {
	int ix = GetPos(x,y);
	return weight_gradients[ix];
}

void Mat::SetGradient(int x, int y, double v) {
	int ix = GetPos(x,y);
	weight_gradients[ix] = v;
}

void Mat::AddGradient(int x, int y, double v) {
	int ix = GetPos(x,y);
	weight_gradients[ix] += v;
}

void Mat::ZeroGradients() {
	for(int i = 0; i < weight_gradients.GetCount(); i++)
		weight_gradients[i] = 0.0;
}

void Mat::AddFrom(const Mat& volume) {
	for (int i = 0; i < weights.GetCount(); i++) {
		weights[i] += volume.Get(i);
	}
}

void Mat::AddGradientFrom(const Mat& volume) {
	for (int i = 0; i < weight_gradients.GetCount(); i++) {
		weight_gradients[i] += volume.GetGradient(i);
	}
}

void Mat::AddFromScaled(const Mat& volume, double a) {
	for (int i = 0; i < weights.GetCount(); i++) {
		weights[i] += a * volume.Get(i);
	}
}

void Mat::SetConst(double c) {
	for (int i = 0; i < weights.GetCount(); i++) {
		weights[i] = c;
	}
}

void Mat::SetConstGradient(double c) {
	for (int i = 0; i < weight_gradients.GetCount(); i++) {
		weight_gradients[i] = c;
	}
}

double Mat::Get(int i) const {
	return weights[i];
}

double Mat::GetGradient(int i) const {
	return weight_gradients[i];
}

void Mat::SetGradient(int i, double v) {
	weight_gradients[i] = v;
}

void Mat::AddGradient(int i, double v) {
	weight_gradients[i] += v;
}

void Mat::Set(int i, double v) {
	weights.Set(i, v);
}

#define STOREVAR(json, field) map.GetAdd(#json) = this->field;
#define LOADVAR(field, json) this->field = map.GetValue(map.Find(#json));
#define LOADVARDEF(field, json, def) {Value tmp = map.GetValue(map.Find(#json)); if (tmp.IsNull()) this->field = def; else this->field = tmp;}

void Mat::Store(ValueMap& map) const {
	STOREVAR(sx, width);
	STOREVAR(sy, height);
	
	Value w;
	for(int i = 0; i < weights.GetCount(); i++) {
		double value = weights[i];
		w.Add(value);
	}
	map.GetAdd("w") = w;
	
	Value dw;
	for(int i = 0; i < weight_gradients.GetCount(); i++) {
		double value = weight_gradients[i];
		dw.Add(value);
	}
	map.GetAdd("dw") = dw;
}

void Mat::Load(const ValueMap& map) {
	LOADVAR(width, sx);
	LOADVAR(height, sy);
	
	length = width * height;
	
	weights.SetCount(0);
	weights.SetCount(length, 0);
	weight_gradients.SetCount(0);
	weight_gradients.SetCount(length, 0);
	
	// copy over the elements.
	Value w = map.GetValue(map.Find("w"));
	
	for (int i = 0; i < length; i++) {
		double value = w[i];
		weights.Set(i, value);
	}
	
	int i = map.Find("dw");
	if (i != -1) {
		Value dw = map.GetValue(i);
		for (int i = 0; i < length; i++) {
			double value = dw[i];
			weight_gradients[i] = value;
		}
	}
}








void MatPool::InitMat(MatId& id, int width, int height, double default_value) {
	lock.Enter();
	id.value = mats.GetCount();
	Mat& mat = mats.Add();
	mat.Init(width, height, default_value);
	lock.Leave();
}

void MatPool::InitMat(MatId& id) {
	lock.Enter();
	id.value = mats.GetCount();
	Mat& mat = mats.Add();
	lock.Leave();
}

void MatPool::RandMat(int n, int d, double mu, double std, MatId& out) {
	lock.Enter();
	out.value = mats.GetCount();
	Mat& mat = mats.Add();
	FastDQL::RandMat(n, d, mu, std, mat);
	lock.Leave();
}

MatId MatPool::AddTempMat(Mat& mat) {
	lock.Enter();
	MatId id;
	id.value = tmp_mat.GetCount() * -1 -2;
	tmp_mat.Add(&mat);
	lock.Leave();
	return id;
}

void MatPool::ClearTempMat() {
	tmp_mat.SetCount(0);
}

Mat& MatPool::Get(const MatId& id) {
	if (id.value >= 0)
		return mats[id.value];
	if (id.value == -1)
		Panic("Invalid MatId");
	int pos = id.value * -1 - 2;
	return *tmp_mat[pos];
}

int MatPool::GetInput(int pos) {
	return index_sequence[pos];
}


}
