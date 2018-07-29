/* This is the main file for the `whoosh` interpreter and the part
   that you modify. */

   #include <stdlib.h>
   #include <stdio.h>
   #include "csapp.h"
   #include "ast.h"
   #include "fail.h"
   
   static void run_script(script *scr);
   static void run_group(script_group *group);
   static void run_command(script_command *command);
   static void set_var(script_var *var, int new_value);
   static void read_to_var(int fd, script_var *var);
   static void write_var_to(int fd, script_var *var);
   
   static volatile gid_t gid_current;
   static sigset_t sigs;

   /* You probably shouldn't change main at all. */
   
   static void ctl_c_handling(int sigchld)
   {
     killpg(gid_current, SIGTERM);
     killpg(gid_current, SIGCONT);
   }

   int main(int argc, char **argv) {
     script *scr;
     
     if ((argc != 1) && (argc != 2)) {
       fprintf(stderr, "usage: %s [<script-file>]\n", argv[0]);
       exit(1);
     }
   
     scr = parse_script_file((argc > 1) ? argv[1] : NULL);
   
     run_script(scr);
   
     return 0;
   }
   
   static void run_script(script *scr) {
     Sigemptyset(&sigs);
     Sigaddset(&sigs, SIGINT);

     Signal(SIGINT, ctl_c_handling);
     int i;
     for(i = 0; i < scr->num_groups; i++)
     {
       run_group(&scr->groups[i]);
     }
   }
   
   int index_of_command(int pids[], int current_pid, int num_commands)
   {
     int i;
     for(i = 0; i < num_commands; i++)
     {
       if(pids[i] == current_pid)
       {
         return i;
       } 
     }
     return -1;
   }
   
   static void run_group(script_group *group) {
     /* You'll have to make run_group do better than this, too */
     if(group->mode == 0 || group->mode == 1)
     {
       int i;
       int status;
       for(i = 0; i < group->repeats; i++)
       {
         int j;
         int pids[group->num_commands];
         //Sigprocmask(SIG_BLOCK, &sigs, NULL);
         for(j = 0; j < group->num_commands; j++)
         {
           int fds_out[2];
           int fds_in[2];
           Pipe(fds_out);
           Pipe(fds_in);
           pid_t pid = Fork();
           pids[j] = pid;
           
           if(pid != 0)
           {
             if (group->commands[j].pid_to != NULL)
             {
               set_var(group->commands[j].pid_to, pid);
             }

             if(j == 0)
             {
               gid_current = pid;
             }
             setpgid(pid, gid_current);
           }
           if(pid == 0)
           {
             if (group->commands[j].input_from != NULL)
             {
               Dup2(fds_in[0], 0);
             }
             Close(fds_in[1]);
             
             if (group->commands[j].output_to != NULL)
             {
               Dup2(fds_out[1], 1);
             }
             Close(fds_out[0]);
             run_command(&group->commands[j]);
           }
           if (group->commands[j].input_from != NULL)
           {
             Close(fds_in[0]);
             write_var_to(fds_in[1], group->commands[j].input_from);
             Close(fds_in[1]);
           }
           else
           {
             Close(fds_in[0]);
             Close(fds_in[1]);
           }
           if (group->commands[j].output_to != NULL)
           {
             Close(fds_out[1]);
             group->commands[j].extra_data = (void *)fds_out;
             read_to_var(fds_out[0], group->commands[j].output_to);
             Close(fds_out[0]);
           }
           else
           {
             Close(fds_out[1]);
             Close(fds_out[0]);
           }
         }
         //Sigprocmask(SIG_UNBLOCK, &sigs, NULL);
         int k;
         for(k = 0; k < group->num_commands; k++)
         {
           pid_t pid = Wait(&status);
           int index_command = index_of_command(pids, pid, group->num_commands);
           /*if (group->commands[index_command].output_to != NULL && group->commands[index_command].extra_data != NULL)
           {
             printf("here\n");
             int* fds_out = (int*)(group->commands[index_command].extra_data);
             read_to_var(fds_out[0], group->commands[index_command].output_to);
             Close(fds_out[0]);
           }*/
           if(WIFEXITED(status))
           {
             int exit_status = WEXITSTATUS(status);
             if(exit_status != 0 && group->commands[index_command].output_to != NULL)
             {
               set_var(group->commands[index_command].output_to, exit_status);
             }
           }
           else
           {
             int signal_status = WIFSIGNALED(status);
             if(signal_status != 0)
             {
               int signal_value = -WTERMSIG(status);
               if(group->commands[index_command].output_to != NULL)
               {
                set_var(group->commands[index_command].output_to, signal_value);
               }
             }
           }
         }
       }
     }
     else //or case
     {
      int i;
      int status;
      for(i = 0; i < group->repeats; i++)
      {
        int j;
        int pids[group->num_commands];
        for(j = 0; j < group->num_commands; j++)
        {
          int fds_out[2];
          int fds_in[2];
          Pipe(fds_out);
          Pipe(fds_in);
          pid_t pid = Fork();
          pids[j] = pid;
          if(pid != 0)
          {
            if (group->commands[j].pid_to != NULL)
            {
              set_var(group->commands[j].pid_to, pid);
            }
          }
          if(j == 0)
          {
            gid_current = pid;
          }
          setpgid(pid, gid_current);
          if(pid == 0)
          {        
            if (group->commands[j].input_from != NULL)
            {
              Dup2(fds_in[0], 0);
            }
            Close(fds_in[1]);
        
            if (group->commands[j].output_to != NULL)
            {
              Dup2(fds_out[1], 1);
            }
            Close(fds_out[0]);
            run_command(&group->commands[j]);
          }
          if (group->commands[j].input_from != NULL)
          {
            Close(fds_in[0]);
            write_var_to(fds_in[1], group->commands[j].input_from);
            Close(fds_in[1]);
          }
          else
          {
            Close(fds_in[0]);
            Close(fds_in[1]);
          }
          if (group->commands[j].output_to != NULL)
          {
            Close(fds_out[1]);
            group->commands[j].extra_data = (void *)fds_out;
            //read_to_var(fds_out[0], group->commands[j].output_to);
            Close(fds_out[0]);
          }
          else
          {
            Close(fds_out[1]);
            Close(fds_out[0]);
          }
        }
        pid_t pid_finished = Wait(&status);
        
        int index_command = index_of_command(pids, pid_finished, group->num_commands);
        if (group->commands[index_command].output_to != NULL && group->commands[index_command].extra_data != NULL)
        {
          int fds_out[2];
          //fds_out[0] = ((int*)group->commands[index_command].extra_data)[0];
          //printf("start\n");
          //read_to_var(fds_out[0], group->commands[index_command].output_to);
          //printf("after read to var\n");
          //Close(fds_out[0]);
        }
        if(WIFEXITED(status))
        {
          int exit_status = WEXITSTATUS(status);
          if(exit_status != 0 && group->commands[index_command].output_to != NULL)
          {
            set_var(group->commands[index_command].output_to, exit_status);
          }
        }
        else
        {
          int signal_status = WIFSIGNALED(status);
          if(signal_status != 0)
          {
            int signal_value = -WTERMSIG(status);
            if(group->commands[index_command].output_to != NULL)
            {
              set_var(group->commands[index_command].output_to, signal_value);
            }
          }
        }
        int k;
        for(k = 0; k < group->num_commands; k++)
        {
          if(pids[k] != pid_finished)
          {
            pid_t pid_to_kill = kill(pids[k], SIGKILL);
            waitpid(pid_to_kill, &status, 0);
            waitpid(pids[k], &status, 0);
            int command_killed = index_of_command(pids, pids[k], group->num_commands);
            if(WIFEXITED(status))
            {
              int exit_status = WEXITSTATUS(status);
              if(exit_status != 0 && group->commands[command_killed].output_to != NULL)
              {
                set_var(group->commands[command_killed].output_to, exit_status);
              }
            }
            else
            {
              int signal_status = WIFSIGNALED(status);
              if(signal_status != 0)
              {
                int signal_value = -WTERMSIG(status);
                if(group->commands[command_killed].output_to != NULL)
                {
                  set_var(group->commands[command_killed].output_to, signal_value);
                }
              }
            }
          }
        }
      }
    }
   }
   
   /* This run_command function is a good start, but note that it runs
      the command as a replacement for the `whoosh` script, instead of
      creating a new process. */
   
   static void run_command(script_command *command) {
     const char **argv;
     int i;
   
     argv = malloc(sizeof(char *) * (command->num_arguments + 2));
     argv[0] = command->program;
     
     for (i = 0; i < command->num_arguments; i++) {
       if (command->arguments[i].kind == ARGUMENT_LITERAL)
         argv[i+1] = command->arguments[i].u.literal;
       else
         argv[i+1] = command->arguments[i].u.var->value;
     }
     
     argv[command->num_arguments + 1] = NULL;
     Execve(argv[0], (char * const *)argv, environ);
   }
   
   /* You'll likely want to use this set_var function for converting a
      numeric value to a string and installing it as a variable's
      value: */
   static void set_var(script_var *var, int new_value) {
     char buffer[32];
     free((void*)var->value);
     snprintf(buffer, sizeof(buffer), "%d", new_value);
     var->value = strdup(buffer);
   }
   
   /* You'll likely want to use this write_var_to function for writing a
      variable's value to a pipe: */
   static void write_var_to(int fd, script_var *var) {
     size_t len = strlen(var->value);
     ssize_t wrote = Write(fd, var->value, len);
     wrote += Write(fd, "\n", 1);
     if (wrote != len + 1)
       app_error("didn't write all expected bytes");
   }
   
   /* You'll likely want to use this read_var_to function for reading a
      pipe's content into a variable: */
   static void read_to_var(int fd, script_var *var) {
     size_t size = 4097, amt = 0;
     char buffer[size];
     ssize_t got;
   
     while (1) {
       got = Read(fd, buffer + amt, size - amt);
       if (!got) {
         if (amt && (buffer[amt-1] == '\n'))
           amt--;
         buffer[amt] = 0;
         free((void*)var->value);
         var->value = strdup(buffer);
         return;
       }
       amt += got;
       if (amt > (size - 1))
         app_error("received too much output");
     }
   }