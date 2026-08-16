#include "Engine/Entities/Variable.hpp"

#include <cassert>
#include <cstdint>

int main() {
	ArrayVariable source;
	source.no      = 17;
	source.num_dim = 2;
	source.dim[0]  = 2;
	source.dim[1]  = 3;
	source.data    = new int32_t[6]{1, 2, 3, 4, 5, 6};

	ArrayVariable copied = source;
	assert(copied.no == 17);
	assert(copied.num_dim == 2);
	assert(copied.data != source.data);
	for (int i = 0; i < 6; ++i)
		assert(copied.data[i] == source.data[i]);

	copied.data[0] = 99;
	assert(source.data[0] == 1);

	ArrayVariable zeroIndexReference;
	zeroIndexReference.no      = 18;
	zeroIndexReference.num_dim = 2;
	zeroIndexReference.dim[0]  = 0;
	zeroIndexReference.dim[1]  = 4;

	ArrayVariable copiedReference = zeroIndexReference;
	assert(copiedReference.no == 18);
	assert(copiedReference.num_dim == 2);
	assert(copiedReference.dim[0] == 0);
	assert(copiedReference.dim[1] == 4);
	assert(copiedReference.data == nullptr);
}
