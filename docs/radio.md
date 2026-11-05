# Radio

Cada emisora es un fichero de `assets/audio/radio/` (`01_pop.mp3` … `10_ambient.mp3`; el orden y los nombres están en `app/engine/audio/radio.hpp`). El formato recomendado es **MP3** (estéreo, 128 kbps): ocupa poco, se oye bien y se descodifica sobre la marcha (`minimp3`), así que puede durar horas sin gastar memoria. También se admite WAV con el mismo nombre.

Para una emisora tipo radio real (canciones con comentarios entre medias), une los MP3 con ffmpeg:

```
ffmpeg -f concat -safe 0 -i lista.txt -ar 44100 -ac 2 -b:a 128k 01_pop.mp3
```

donde `lista.txt` tiene una línea `file 'cancion1.mp3'` por cada pista y comentario, en orden. Al cambiar de emisora se guarda por dónde iba y al volver sigue desde ahí; al llegar al final empieza de nuevo.
