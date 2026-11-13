// spotify.h - Spotify "now playing". Get the refresh token once with tools/spotify_auth.py.
#pragma once
#include "model.h"

bool spotifyConfigured();
// true = request worked (np.playing may still be false when nothing is playing)
bool spotifyNowPlaying(NowPlaying &np);
