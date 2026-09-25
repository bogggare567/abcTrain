// Drives the phone page of a RoomDemo room in Chromium at phone size:
// joins, answers a discrete and a continuous round, sees the answers and
// the place. Screenshots go to the folder in argv[3].
//   ./RoomDemo 18931 45 & node tools/room_demo_check.mjs 18931 <room code> <out dir>
import { chromium } from "playwright";
const [port, room, out = "/tmp/phone"] = process.argv.slice(2);
const browser = await chromium.launch({ executablePath: "/opt/pw-browsers/chromium" });
const page = await browser.newPage({ viewport: { width: 390, height: 844 }, deviceScaleFactor: 2 });
const errors = [];
page.on("pageerror", (e) => errors.push(String(e)));
const shot = (n) => page.screenshot({ path: `${out}/${n}.png` });
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

await page.goto(`http://127.0.0.1:${port}/?r=${room}`);
await page.fill("#name", "Богдан");
await shot("1-join");
await page.click("#go");
await page.waitForSelector("text=Богдан");
await shot("2-lobby");
await page.waitForSelector(".choice", { timeout: 15000 });
await page.click('.choice[data-i="2"]');
await wait(1200);
await shot("3-round-choices");
await page.waitForSelector(".choice.right", { timeout: 15000 });
await shot("4-reveal-right");
await page.waitForSelector("#scale", { timeout: 15000 });
const box = await (await page.$("#scale")).boundingBox();
await page.mouse.move(box.x + box.width * 0.3, box.y + box.height / 2);
await page.mouse.down();
await page.mouse.move(box.x + box.width * 0.56, box.y + box.height / 2, { steps: 8 });
await page.mouse.up();
await wait(1200);
await shot("5-round-scale");
await page.waitForSelector("text=900 Hz", { timeout: 15000 });
await shot("6-reveal-scale");
await page.waitForSelector(".big", { timeout: 15000 });
await shot("7-finished");
const text = await page.innerText("main");
console.log(JSON.stringify({ errors, final: text }));
await browser.close();
