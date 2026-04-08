# CAGE

This is a simple coding agent based on Thorsten Ball's excellent article, [How to Build an Agent](https://ampcode.com/notes/how-to-build-an-agent), developed in C (just for fun).

For transparency, I've included the AGENTS.md file I used when developing this project. Since this was a learning project, I wanted my IDE agent provide guidance and code review without writing the code for me. I had AI generate the file based on what I wanted to get out of this project, and for code style/correctness I asked it to help me aim at NASA's [The Power of 10: Rules for Developing Safety-Critical Code](https://web.eecs.umich.edu/~imarkov/10rules.pdf)(Gerard J. Holzmann, NASA/JPL). Aside from some tab-based autocomplete, I believe I typed just about every character in this project by hand.

### A note from Taylor before starting the project: 
I chose this project because:
- it will require me to handle a lot of dynamically allocated strings.
- I might have to write my own JSON parser.
- I'll have to do some socket programming to make the calls to the LLM, especially if I want to support streaming mode. 
- I might have to do some multi-processing or multithreading if I want to make this more like the kind of dynamic TUI application we're all used to by now, where we'd be able to interrupt the agent, add context, etc.
- Some kind of logging?
- I plan to make sure all the functionality is suitably tested in code. 

These just seem like some good intermediate C skills to have to figure out.

# Prereqs

Following along with Thorsten's article, I just plan to use the anthropic API. You'll need to set your API key as an environment variable before you run the program:

```bash
EXPORT ANTHROPIC_API_KEY=<your api key>
./cage
```
