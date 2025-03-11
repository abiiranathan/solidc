package pipline

/*
#cgo LDFLAGS: -L. -lpipeline
#include "pipeline.h"
*/
import "C"
import (
	"errors"
	"unsafe"
)

// Command represents a command in the pipeline.
type Command struct {
	args []string
}

// Pipeline represents a sequence of commands.
type Pipeline struct {
	commands []*Command
}

// NewCommand creates a new Command with the given arguments.
func NewCommand(args []string) *Command {
	return &Command{args: args}
}

// NewPipeline creates a new Pipeline.
func NewPipeline() *Pipeline {
	return &Pipeline{}
}

// AddCommand adds a command to the pipeline.
func (p *Pipeline) AddCommand(cmd *Command) *Pipeline {
	p.commands = append(p.commands, cmd)
	return p
}

// Run executes the pipeline and captures the output of the last command.
// Redirect stdout to outputFd. If its -1, output is not redirected.
func (p *Pipeline) Run(outputFd int) error {
	if len(p.commands) == 0 {
		return errors.New("no commands in pipeline")
	}

	// Convert Go commands to C commands
	cCommands := make([]*C.CommandNode, len(p.commands))
	for i, cmd := range p.commands {
		// Convert Go []string to C char**
		cArgs := make([]*C.char, len(cmd.args)+1)
		for j, arg := range cmd.args {
			cArgs[j] = C.CString(arg)
		}
		cArgs[len(cmd.args)] = nil // NULL-terminate the array

		// Create a C CommandNode
		cCommands[i] = C.create_command_node((**C.char)(unsafe.Pointer(&cArgs[0])))
	}

	// NULL-terminate the array of C CommandNode pointers
	cCommands = append(cCommands, nil)

	// Build the pipeline
	C.build_pipeline((**C.CommandNode)(unsafe.Pointer(&cCommands[0])))

	// Execute the pipeline
	C.execute_pipeline(cCommands[0], C.int(outputFd))

	// Free the C CommandNodes
	C.free_pipeline(cCommands[0])

	return nil
}
