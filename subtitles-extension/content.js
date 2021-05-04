let observer = new MutationObserver(onMutation);

function onMutation(objects, observer) {
    console.log("onMutation");
    let subtitles = [];

    for (let i = 0; i < objects.length; i++) {
        const record = objects[i];
        if (record.target.className.includes("player-timedtext")) {
            const timedText = record.target;
            const lines = timedText.firstElementChild?.children;
            if (!lines) break;
            for (let span of lines) {
                subtitles.push(span.innerText);
            }
            break;
        }
    }

    fetch("http://localhost:4007/subtitles", {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*'
        },
        body: JSON.stringify(subtitles.join(""))
    });
}

function hook() {
    console.log("hook...");
    const timedText = document.getElementsByClassName('player-timedtext')[0];
    if (!timedText) {
        setTimeout(hook, 1000);
        return;
    }

    console.log("starting observer");
    observer.observe(timedText, { childList: true });
}

hook();