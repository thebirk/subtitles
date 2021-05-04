const DEBUG = false;
let observer = new MutationObserver(onMutation);

function onMutation(objects, observer) {
    if (DEBUG) console.log("onMutation");
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
        mode: 'no-cors',
        headers: {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*'
        },
        body: JSON.stringify(subtitles.join(""))
    });
}

function observerObserver() {
    if (DEBUG) console.log("observerObserver");
    const timedText = document.getElementsByClassName('player-timedtext')[0];
    if (!timedText) {
        if (DEBUG) console.log("observerObserver - timedText is dead");
        observer.disconnect();
        setTimeout(hook, 100);
        return;
    }
    if (DEBUG) console.log("observerObserver - we go again");
    setTimeout(observerObserver, 1000);
}

function hook() {
    if (DEBUG) console.log("hook...");
    const timedText = document.getElementsByClassName('player-timedtext')[0];
    if (!timedText) {
        setTimeout(hook, 1000);
        return;
    }

    if (DEBUG) console.log("starting observer");
    observer.observe(timedText, { childList: true });
    setTimeout(observerObserver, 1000);
}

hook();