/**
 *  Pool.cpp
 *  ONScripter-RU
 *
 *  Contains graphics pools for load and preserve.
 *
 *  Consult LICENSE file for licensing terms and copyright holders.
 */

#include "Engine/Graphics/Pool.hpp"
#include "Engine/Graphics/PNG.hpp"
#include "Engine/Components/Async.hpp"
#include "Support/FileDefs.hpp"

SDL_Surface *TempImagePool::getImage() {
	Lock lock(this);
	SDL_Surface *r = nullptr;
	while (!freeImages.empty()) {
		SDL_Surface *candidate = freeImages.back();
		freeImages.pop_back();
		auto it = pool.find(candidate);
		if (it != pool.end() && !it->second) {
			r = candidate;
			break;
		}
	}

	if (!r) {
		r = onsCreateRGBSurface(SDL_SWSURFACE, size.x, size.y, 24,
		                        0x000000ff, 0x0000ff00, 0x00ff0000, 0);
	}
	pool[r] = true;
	return r;
}

void TempImagePool::giveImage(SDL_Surface *im) {
	Lock lock(this);
	auto it = pool.find(im);
	if (it == pool.end() || it->second) {
		pool[im] = false;
		freeImages.push_back(im);
	}
}

void TempImagePool::addImages(int n) {
	Lock lock(this);
	SDL_Surface *im;
	for (int i = 0; i < n; i++) {
		im       = onsCreateRGBSurface(SDL_SWSURFACE, size.x, size.y, 24,
                                  0x000000ff, 0x0000ff00, 0x00ff0000, 0);
		pool[im] = false;
		freeImages.push_back(im);
	}
}

TempImagePool::~TempImagePool() {
	for (auto diver : pool) {
		if (!diver.second) {
			SDL_FreeSurface(diver.first);
		} else {
			sendToLog(LogLevel::Error, "~TempImagePool@Diver cannot be eaten\n");
		}
	}
}

PNGLoader *TempImageLoaderPool::getLoader() {
	Lock lock(this);
	PNGLoader *r = nullptr;
	while (!freeLoaders.empty()) {
		PNGLoader *candidate = freeLoaders.back();
		freeLoaders.pop_back();
		auto it = pool.find(candidate);
		if (it != pool.end() && !it->second) {
			r = candidate;
			break;
		}
	}

	if (!r)
		r = new PNGLoader();

	pool[r] = true;
	return r;
}

void TempImageLoaderPool::giveLoader(PNGLoader *ldr) {
	Lock lock(this);
	auto it = pool.find(ldr);
	if (it == pool.end() || it->second) {
		pool[ldr] = false;
		freeLoaders.push_back(ldr);
	}
}

void TempImageLoaderPool::addLoaders(int n) {
	Lock lock(this);
	PNGLoader *ldr;
	for (int i = 0; i < n; i++) {
		ldr       = new PNGLoader();
		pool[ldr] = false;
		freeLoaders.push_back(ldr);
	}
}

TempImageLoaderPool::~TempImageLoaderPool() {
	for (auto diver : pool) {
		if (!diver.second) {
			delete diver.first;
		} else {
			sendToLog(LogLevel::Error, "~TempImageLoaderPool@Diver cannot be eaten\n");
		}
	}
}

TempImageLoaderPool pngImageLoaderPool;
