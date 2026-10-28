# FL Studio

FL Studio opens samples in HARP as an external audio editor.

## Link HARP

1. Choose `Options > File settings` (`F10`).
2. Under `External Tools`, click the file icon beside `File`, and select HARP.
3. Select HARP in the list of tools, and enable `Launch with file` and `Default Editor`. HARP then
   replaces Edison wherever FL Studio offers to open audio in an editor.

<!-- TODO: screenshot. The File settings of FL Studio, with HARP added under External Tools and set
     as the default editor.
![HARP set as the default editor under External Tools in FL Studio](/images/flstudio-external-tools.png)
-->

The manual of Image-Line describes the setting under
[File Settings](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/envsettings_files.htm).

## Open Audio in HARP

HARP overwrites the file of the sample, wherever it is, including a sample from a pack. Copy the file
first if you want to keep the original.

1. In the Channel settings of a sampler channel, choose `Edit...` (`E`) from the `Sample` menu,
   which opens the sample in HARP.
2. Process the DAW-linked track and send the result back, as in
   [The Round Trip](/content/setup/#the-round-trip).
3. Choose `Reload` from the same `Sample` menu, which loads the changed file.
