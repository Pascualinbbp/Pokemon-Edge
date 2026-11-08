# Radio

Cada emisora es una fila de la tabla `radio` (`id`, `name`, `genre`) y dos ficheros: la música `assets/audio/radio/<id>.mp3` y el logo `assets/sprites/radio/<id>.png`. El formato recomendado es **MP3** (estéreo, 128 kbps): ocupa poco, se oye bien y se descodifica sobre la marcha (`minimp3`), así que puede durar horas sin gastar memoria. También se admite WAV con el mismo nombre.

Para una emisora tipo radio real (canciones con comentarios entre medias), une los MP3 con ffmpeg:

```
ffmpeg -f concat -safe 0 -i lista.txt -ar 44100 -ac 2 -b:a 128k 1.mp3
```

donde `lista.txt` tiene una línea `file 'cancion1.mp3'` por cada pista y comentario, en orden. Todas las emisoras suenan a la vez según un reloj global (`AudioEngine::broadcastSeconds()`): al sintonizar una, empieza en la posición `reloj mod duración` (las más cortas se repiten) y las que no se escuchan no consumen nada. Un hilo (`RadioStream`) descodifica y rellena un búfer circular (capacidad 4 s, objetivo 2 s); el callback de audio solo copia y remuestrea, así que un parón del disco o del descodificador no corta el sonido. El hilo del dispositivo tiene prioridad alta y 6 búferes.

Registro (`RADIO` en el log): cada cambio de emisora (posición de inicio), cada 30 s el estado (búfer, consumo, retrasos de relleno, hueco máximo, tiempo de mezcla) y, de inmediato y con límite de frecuencia, cada vaciado del búfer (underrun). Si la radio se entrecorta en una sesión larga, esas líneas dicen si falló el descodificador, el disco o el dispositivo de audio.
