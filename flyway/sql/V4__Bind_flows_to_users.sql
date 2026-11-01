-- Add owner column to flows table (each flow has one user who owns it)
ALTER TABLE flows
ADD COLUMN owner_id UUID NOT NULL;

-- Add foreign key constraint for owner
ALTER TABLE flows
ADD CONSTRAINT fk_flows_owner
FOREIGN KEY (owner_id) REFERENCES users(id) ON DELETE CASCADE;

-- Create shared_flows table for many-to-many relationship between flows and shared users
CREATE TABLE shared_flows (
    flow_id UUID NOT NULL,
    user_id UUID NOT NULL,
    shared_at TIMESTAMP WITH TIME ZONE DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (flow_id, user_id),
    FOREIGN KEY (flow_id) REFERENCES flows(id) ON DELETE CASCADE,
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
);

-- Create index on owner_id for faster queries
CREATE INDEX idx_flows_owner_id ON flows(owner_id);

-- Create index on flow_id for shared_flows
CREATE INDEX idx_shared_flows_flow_id ON shared_flows(flow_id);

-- Create index on user_id for shared_flows
CREATE INDEX idx_shared_flows_user_id ON shared_flows(user_id);
