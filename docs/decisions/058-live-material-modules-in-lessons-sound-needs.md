# 058 — One live concert under the lessons; modules inside the lessons window; exercises that need hits

Date: 2026-10-02. Status: accepted (Bogdan, after trying 2.0: «с живой записью
прям слышно как меняется вокал … не нужно пихать миксы если у них нет
голоса»; «окно уроков залезает на студию»; «модули — нужны ли они отдельно»;
«задержку нельзя на гитаре или тянущемся звуке»).

## Lesson material

Every lesson material now comes from the same two moments of Bogdan's live
multitrack as the vocal phrases: the voice alone, the kick (in + out), the
snare (top + bottom, polarity fixed), the drums, the bass, the guitar, and a
stereo mix of all of them — with the voice in it. Synthesized material is
only the fallback for a name with no recording (chord, keys, pink, hit).
`LessonAudioBed::renderLive`. The live vocal and the live mix are also the
"Built-in Voice" category, which is what the app's Studio plays until the
player picks something else (in a DAW the default stays the host's audio).

## The lessons window

- The course list folds away (the ‹ button in the header; remembered): the
  window then is only the reading column and sits beside the Studio.
- The plugins' modules (watch → try → done, ADR 037) are the first section
  of the list. The Studio's editor lends its module screen to the window
  and takes it back; the plugin's own "Modules" button and companion window
  are not shown in the app any more. One place for everything that teaches.

## Sounds that suit the exercise

A chosen clip or pack used to play in every exercise. Delay, reverb and
compression are heard on hits with space between them; on a held chord an
echo or a tail disappears. The library now measures how sustained a clip
is when it is chosen (median 20 ms level over the 95th percentile); those
three exercises skip a clip above 0.35 and play their own material, and the
Sounds page says so. The other exercises play any clip.
