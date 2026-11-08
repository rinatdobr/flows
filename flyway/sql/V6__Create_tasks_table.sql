-- Create tasks table
CREATE TABLE tasks (
    id UUID PRIMARY KEY DEFAULT gen_random_uuid(),
    name VARCHAR(255) NOT NULL,
    description TEXT NOT NULL,
    start_at TIMESTAMP WITH TIME ZONE,
    end_at TIMESTAMP WITH TIME ZONE
);

-- Create indexes for faster queries
CREATE INDEX idx_tasks_name ON tasks(name);
CREATE INDEX idx_tasks_start_at ON tasks(start_at);
CREATE INDEX idx_tasks_end_at ON tasks(end_at);
