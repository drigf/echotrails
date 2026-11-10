/* Run using File > Scripts > Run Script File in After Effects after installing EchoTrails.plugin. */
(function () {
    var installed = false;
    for (var i = 0; i < app.effects.length; i++) {
        if (app.effects[i].matchName === "Signal Forge Echo Trails") { installed = true; break; }
    }
    if (!installed) { alert("Install EchoTrails.plugin and restart After Effects first."); return; }
    var musicFile = File.openDialog("Choose music for the Echo Trails demo (Cancel uses animated Manual Drive)");
    app.beginUndoGroup("Create Echo Trails Demo");
    try {
        if (!app.project) app.newProject();
        var folder = app.project.items.addFolder("Echo Trails Demo");
        var source = app.project.items.addComp("Echo Trails - Animated Source", 1280, 720, 1, 12, 30);
        source.parentFolder = folder;
        var shape = source.layers.addShape();
        shape.name = "Moving Signal Ring";
        var contents = shape.property("ADBE Root Vectors Group");
        var ellipse = contents.addProperty("ADBE Vector Shape - Ellipse");
        ellipse.property("ADBE Vector Ellipse Size").setValue([150, 150]);
        var stroke = contents.addProperty("ADBE Vector Graphic - Stroke");
        stroke.property("ADBE Vector Stroke Color").setValue([1, 1, 1]);
        stroke.property("ADBE Vector Stroke Width").setValue(12);
        shape.property("ADBE Transform Group").property("ADBE Position").expression =
            "[500 + 220*Math.sin(time*1.7), 360 + 170*Math.sin(time*2.4)]";
        var comp = app.project.items.addComp("Echo Trails - Music Demo", 1280, 720, 1, 12, 30);
        comp.parentFolder = folder;
        comp.layers.addSolid([.008,.012,.025], "Dark Background", 1280, 720, 1, 12);
        var music = null;
        if (musicFile) {
            var footage = app.project.importFile(new ImportOptions(musicFile));
            footage.parentFolder = folder;
            music = comp.layers.add(footage);
            // Hide any video stream while keeping the audio switch enabled.
            if (music.hasVideo) music.enabled = false;
        }
        var layer = comp.layers.add(source);
        layer.name = "ECHO TRAILS - edit Effect Controls";
        var effect = layer.property("ADBE Effect Parade").addProperty("Signal Forge Echo Trails");
        if (music) effect.property("Music Layer").setValue(music.index);
        else effect.property("Manual Drive (%)").expression = "Math.pow(Math.max(0, Math.sin(time*Math.PI*4)), 6)*85";
        effect.property("Echo Count").setValue(16);
        effect.property("Echo Delay (seconds)").setValue(.045);
        effect.property("Echo Retention (%)").setValue(91);
        effect.property("Offset X per Echo (px)").setValue(8);
        comp.time = 3;
        comp.openInViewer();
        layer.selected = true;
    } catch (error) { alert("Echo Trails demo: " + error.toString()); }
    finally { app.endUndoGroup(); }
}());
