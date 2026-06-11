// ABOUTME: Local gerund list for the MacCode ✻ verb line.
// ABOUTME: The Agent SDK doesn't surface Claude Code's flavor verbs, so the proxy supplies them.
export const VERBS = [
  "Forging", "Cogitating", "Pondering", "Herding", "Conjuring",
  "Tinkering", "Noodling", "Synthesizing", "Untangling", "Brewing",
];

// Pick a random verb. Pass the previous one to avoid showing the same word
// twice in a row (which reads as the line not having changed).
export function randomVerb(previous?: string): string {
  const choices = previous ? VERBS.filter((v) => v !== previous) : VERBS;
  return choices[Math.floor(Math.random() * choices.length)];
}
