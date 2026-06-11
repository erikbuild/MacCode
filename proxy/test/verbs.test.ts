import { describe, it, expect } from "vitest";
import { VERBS, randomVerb } from "../src/verbs";

describe("randomVerb", () => {
  it("returns a verb from the list", () => {
    for (let i = 0; i < 50; i++) expect(VERBS).toContain(randomVerb());
  });
  it("never repeats the previous verb back-to-back", () => {
    for (let i = 0; i < 100; i++) {
      const v = randomVerb(VERBS[0]);
      expect(VERBS).toContain(v);
      expect(v).not.toBe(VERBS[0]);
    }
  });
});
