# Radio

Cada emisora es una fila de la tabla `radio` (`id`, `name`, `genre`) y dos ficheros: la música `assets/audio/radio/<id>.mp3` y el logo `assets/sprites/radio/<id>.png`. El formato recomendado es **MP3** (estéreo, 128 kbps): ocupa poco, se oye bien y se descodifica sobre la marcha (`minimp3`), así que puede durar horas sin gastar memoria. También se admite WAV con el mismo nombre.

Para una emisora tipo radio real (canciones con comentarios entre medias), une los MP3 con ffmpeg:

```
ffmpeg -f concat -safe 0 -i lista.txt -ar 44100 -ac 2 -b:a 128k 1.mp3
```

donde `lista.txt` tiene una línea `file 'cancion1.mp3'` por cada pista y comentario, en orden. Al cambiar de emisora se guarda por dónde iba y al volver sigue desde ahí; al llegar al final empieza de nuevo.
