# sfxgen regression test

if [ "$1" = "update" ]; then
	sha1sum *.wav >wav.sha1
else
	../sfxgen *.rfx
	sha1sum -c wav.sha1
fi
