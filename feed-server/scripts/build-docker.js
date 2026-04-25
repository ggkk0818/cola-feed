const { spawnSync } = require("node:child_process");

const imageName = process.env.DOCKER_IMAGE_NAME || "cola-feed-server";
const imageTag = process.env.DOCKER_IMAGE_TAG || "latest";
const dockerfilePath = process.env.DOCKERFILE_PATH;
const buildContext = process.env.DOCKER_BUILD_CONTEXT || ".";
const buildNetwork = process.env.DOCKER_BUILD_NETWORK || "host";

const imageRef = `${imageName}:${imageTag}`;
const args = ["build", "-t", imageRef];

if (buildNetwork) {
  args.push("--network", buildNetwork);
}

if (dockerfilePath) {
  args.push("-f", dockerfilePath);
}

args.push(buildContext);

console.log(`[docker-build] docker ${args.join(" ")}`);

const result = spawnSync("docker", args, {
  stdio: "inherit",
});

if (result.error) {
  console.error("[docker-build] Failed to run docker:", result.error.message);
  process.exit(1);
}

process.exit(result.status ?? 1);
